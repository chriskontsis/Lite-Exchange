#include <benchmark/benchmark.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

void ignoreSigpipe() __attribute__((constructor));
void ignoreSigpipe()
{
  signal(SIGPIPE, SIG_IGN);
}

#include "../src/client/FixClient.hpp"
#include "../src/engine/EngineDispatcher.hpp"
#include "../src/fix/FixMessageBuilder.hpp"
#include "../src/gateway/SessionRegistry.hpp"
#include "../src/gateway/SymbolRegistry.hpp"
#include "../src/ipc/FillEvent.hpp"
#include "../src/ipc/MPSC_Queue.hpp"
#include "../src/ipc/OrderEvent.hpp"
#include "../src/net/EventLoop.hpp"
#include "../src/net/IoHandler.hpp"
#include "../src/server/FixServer.hpp"
#include "../src/utility/ThreadAffinity.hpp"
#include "BookSeeder.hpp"
#include "TrafficGenerator.hpp"

struct ServerFixture
{
  std::unique_ptr<MPSC_Queue<ipc::OrderEvent, 65536>> inputQ{
      new MPSC_Queue<ipc::OrderEvent, 65536>()};
  std::unique_ptr<MPSC_Queue<ipc::FillEvent, 65536>> outputQ{
      new MPSC_Queue<ipc::FillEvent, 65536>()};
  gateway::SessionRegistry registry;
  gateway::SymbolRegistry  symbols;
  fix::EngineDispatcher    dispatcher{*inputQ, *outputQ};
  net::EventLoop           loop;
  short                    port_;
  fix::FixServer           server;
  std::atomic<bool>        server_running{true};
  std::thread              server_thread;
  std::atomic<bool>        drain_running{true};
  std::atomic<uint64_t>    fills_received{0};
  std::thread              drain_thread;

  ServerFixture(short port) : port_(port), server{loop, port, *inputQ, registry, symbols}
  {
    server_thread = std::thread(
        [this]()
        {
          net::Event events[64];
          util::pinToCore(2);
          while (server_running.load(std::memory_order_relaxed))
          {
            int n = loop.wait(events, 64, 0);
            for (int i = 0; i < n; ++i)
            {
              auto* handler = static_cast<net::IoHandler*>(events[i].ctx);
              if (events[i].readable)
                handler->onReadable();
              if (events[i].writable)
                handler->onWritable();
              if (handler->wantsClose())
                server.closeSession(handler->fd());
            }
          }
        });
    drain_thread = std::thread(
        [this]()
        {
          util::pinToCore(1);
          ipc::FillEvent fe;
          while (drain_running.load(std::memory_order_relaxed))
          {
            if (outputQ->tryConsume(fe))
            {
              fills_received.fetch_add(1, std::memory_order_relaxed);
              if (auto* session = registry.lookup(fe.session_id_))
                session->sendData(
                    fix::FixMessageBuilder::executionReport(fe, symbols.name(fe.symbol_id_)));
            }
          }
        });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  ~ServerFixture()
  {
    server_running.store(false);
    drain_running.store(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    server_thread.join();
    drain_thread.join();
  }
};

// Each benchmark gets its own port to avoid TIME_WAIT conflicts
static constexpr short PORT_LIMIT_FLOOD = 12400;
static constexpr short PORT_MATCHED_PAIRS = 12401;
static constexpr short PORT_MULTI_CLIENT = 12402;
static constexpr short PORT_FILL_TPUT = 12403;
static constexpr short PORT_LATENCY = 12404;
static constexpr short PORT_CANCEL = 12405;
static constexpr short PORT_DEEP_SWEEP = 12406;
static constexpr short PORT_STEADY = 12407;
static constexpr short PORT_MULTI_SYMBOL  = 12408;
static constexpr short PORT_TICK_TO_TRADE = 12409;

// Scenario 1: limit order send rate (no matches)
static void BM_EndToEnd_LimitFlood(benchmark::State& state)
{
  ServerFixture  srv(PORT_LIMIT_FLOOD);
  fix::FixClient client("127.0.0.1", PORT_LIMIT_FLOOD);
  std::string    msg = fix::FixMessageBuilder::limit<LOB::Side::BUY>(1, 1, 100, "AAPL");

  for (auto _ : state)
    client.send(msg);

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_EndToEnd_LimitFlood)->Iterations(10000);

// Scenario 2: matched pairs — measures full round trip including fill delivery
static void BM_EndToEnd_MatchedPairs(benchmark::State& state)
{
  ServerFixture  srv(PORT_MATCHED_PAIRS);
  fix::FixClient c1("127.0.0.1", PORT_MATCHED_PAIRS);
  fix::FixClient c2("127.0.0.1", PORT_MATCHED_PAIRS);

  LOB::UID uid = 1;
  for (auto _ : state)
  {
    srv.fills_received.store(0, std::memory_order_relaxed);
    c1.send(fix::FixMessageBuilder::limit<LOB::Side::BUY>(uid++, 1, 100, "MSFT"));
    c2.send(fix::FixMessageBuilder::limit<LOB::Side::SELL>(uid++, 1, 100, "MSFT"));

    while (srv.fills_received.load(std::memory_order_relaxed) < 2)
      ;
  }

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_EndToEnd_MatchedPairs)->Iterations(1000);

// Aggregate throughput — N persistent connections, round-robin sends.
static void BM_EndToEnd_MultiClientFlood(benchmark::State& state)
{
  ServerFixture srv(PORT_MULTI_CLIENT);
  const int     num_clients = state.range(0);

  std::vector<std::unique_ptr<fix::FixClient>> clients;
  clients.reserve(num_clients);
  for (int i = 0; i < num_clients; ++i)
    clients.push_back(std::make_unique<fix::FixClient>("127.0.0.1", PORT_MULTI_CLIENT));

  LOB::UID uid = 1;
  for (auto _ : state)
  {
    std::vector<std::thread> threads;
    threads.reserve(num_clients);
    for (int i = 0; i < num_clients; ++i)
    {
      threads.emplace_back(
          [&, i]()
          {
            clients[i]->send(
                fix::FixMessageBuilder::limit<LOB::Side::BUY>(uid + i, 1, 100, "AAPL"));
          });
    }
    for (auto& t : threads)
      t.join();
    uid += num_clients;
  }

  state.SetItemsProcessed(state.iterations() * num_clients);
}
BENCHMARK(BM_EndToEnd_MultiClientFlood)->Arg(2)->Arg(4)->Arg(8)->Iterations(2000);

// Peak fill throughput — N buy + N sell clients, all crossing at the same price.
static void BM_EndToEnd_FillThroughput(benchmark::State& state)
{
  ServerFixture srv(PORT_FILL_TPUT);
  const int     num_pairs = state.range(0);

  std::vector<std::unique_ptr<fix::FixClient>> buyers, sellers;
  for (int i = 0; i < num_pairs; ++i)
  {
    buyers.push_back(std::make_unique<fix::FixClient>("127.0.0.1", PORT_FILL_TPUT));
    sellers.push_back(std::make_unique<fix::FixClient>("127.0.0.1", PORT_FILL_TPUT));
  }

  LOB::UID uid{1};
  for (auto _ : state)
  {
    srv.fills_received.store(0, std::memory_order_relaxed);
    for (int i = 0; i < num_pairs; ++i)
    {
      buyers[i]->send(fix::FixMessageBuilder::limit<LOB::Side::BUY>(uid++, 1, 100, "MSFT"));
      sellers[i]->send(fix::FixMessageBuilder::limit<LOB::Side::SELL>(uid++, 1, 100, "MSFT"));
    }
    while (srv.fills_received.load(std::memory_order_relaxed) <
           static_cast<uint64_t>(num_pairs * 2))
      ;
  }

  state.SetItemsProcessed(state.iterations() * num_pairs * 2);
}
BENCHMARK(BM_EndToEnd_FillThroughput)->Arg(1)->Arg(2)->Arg(4)->Iterations(500);

// Matched pairs with latency distribution — p50/p99/p999 tail latency.
static void BM_EndToEnd_MatchedPairs_Latency(benchmark::State& state)
{
  ServerFixture  srv(PORT_LATENCY);
  fix::FixClient c1("127.0.0.1", PORT_LATENCY);
  fix::FixClient c2("127.0.0.1", PORT_LATENCY);

  LOB::UID uid = 1;
  for (auto _ : state)
  {
    srv.fills_received.store(0, std::memory_order_relaxed);
    c1.send(fix::FixMessageBuilder::limit<LOB::Side::BUY>(uid++, 1, 100, "MSFT"));
    c2.send(fix::FixMessageBuilder::limit<LOB::Side::SELL>(uid++, 1, 100, "MSFT"));
    while (srv.fills_received.load(std::memory_order_relaxed) < 2)
      ;
  }

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_EndToEnd_MatchedPairs_Latency)
    ->Repetitions(200)
    ->Iterations(20)
    ->ReportAggregatesOnly(true)
    ->ComputeStatistics("p50",
                        [](const std::vector<double>& v)
                        {
                          auto c = v;
                          std::sort(c.begin(), c.end());
                          return c[c.size() * 50 / 100];
                        })
    ->ComputeStatistics("p99",
                        [](const std::vector<double>& v)
                        {
                          auto c = v;
                          std::sort(c.begin(), c.end());
                          return c[c.size() * 99 / 100];
                        })
    ->ComputeStatistics("p999",
                        [](const std::vector<double>& v)
                        {
                          auto c = v;
                          std::sort(c.begin(), c.end());
                          return c[c.size() * 999 / 1000];
                        });

// Scenario: realistic message mix — 65% cancels, 35% new limit orders.
static void BM_EndToEnd_CancelPressure(benchmark::State& state)
{
  ServerFixture  srv(PORT_CANCEL);
  fix::FixClient client("127.0.0.1", PORT_CANCEL);

  bench::TrafficConfig cfg;
  cfg.symbol = "AAPL";
  cfg.mid_price = 500;
  cfg.spread = 10;
  cfg.cancel_rate = 0.65;
  bench::TrafficGenerator gen(cfg);

  for (auto _ : state)
    client.send(gen.next(LOB::Side::BUY));

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_EndToEnd_CancelPressure)->Iterations(10000);

// Scenario: pre-seed 20 ask levels x 10 orders, sweep with one market buy.
static void BM_EndToEnd_DeepBookSweep(benchmark::State& state)
{
  ServerFixture  srv(PORT_DEEP_SWEEP);
  fix::FixClient seeder("127.0.0.1", PORT_DEEP_SWEEP, [](std::string_view) {});
  fix::FixClient aggressor("127.0.0.1", PORT_DEEP_SWEEP, [](std::string_view) {});

  bench::SeedConfig scfg;
  scfg.symbol_ = "GOOG";
  scfg.mid_price_ = 500;
  scfg.num_levels_ = 20;
  scfg.orders_per_level_ = 10;
  scfg.drain_wait_ms_ = 150;

  LOB::UID uid = 2'000'000;
  for (auto _ : state)
  {
    state.PauseTiming();
    bench::seedBook(seeder, scfg);
    srv.fills_received.store(0, std::memory_order_relaxed);
    state.ResumeTiming();

    aggressor.send(fix::FixMessageBuilder::market<LOB::Side::BUY>(uid++, 200, "GOOG"));

    while (srv.fills_received.load(std::memory_order_relaxed) < 201)
      ;
  }

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_EndToEnd_DeepBookSweep)->Iterations(50);

// Scenario: 2 market-maker clients + 2 aggressors crossing simultaneously.
static void BM_EndToEnd_SteadyState(benchmark::State& state)
{
  ServerFixture srv(PORT_STEADY);

  bench::TrafficConfig mm_cfg;
  mm_cfg.symbol = "MSFT";
  mm_cfg.mid_price = 500;
  mm_cfg.spread = 10;
  mm_cfg.cancel_rate = 0.65;

  fix::FixClient          mm1("127.0.0.1", PORT_STEADY);
  fix::FixClient          mm2("127.0.0.1", PORT_STEADY);
  fix::FixClient          agg1("127.0.0.1", PORT_STEADY);
  fix::FixClient          agg2("127.0.0.1", PORT_STEADY);
  bench::TrafficGenerator gen1(mm_cfg);
  bench::TrafficGenerator gen2(mm_cfg);

  LOB::UID uid = 3'000'000;
  for (auto _ : state)
  {
    srv.fills_received.store(0, std::memory_order_relaxed);

    mm1.send(gen1.next(LOB::Side::BUY));
    mm2.send(gen2.next(LOB::Side::SELL));

    agg1.send(fix::FixMessageBuilder::limit<LOB::Side::BUY>(uid++, 1, 500, "MSFT"));
    agg2.send(fix::FixMessageBuilder::limit<LOB::Side::SELL>(uid++, 1, 500, "MSFT"));

    while (srv.fills_received.load(std::memory_order_relaxed) < 2)
      ;
  }

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_EndToEnd_SteadyState)->Iterations(2000);

// Scenario: 4 symbols with independent order flow running concurrently.
static void BM_EndToEnd_MultiSymbol(benchmark::State& state)
{
  ServerFixture srv(PORT_MULTI_SYMBOL);

  const std::vector<std::string>               symbols = {"AAPL", "MSFT", "GOOG", "AMZN"};
  std::vector<std::unique_ptr<fix::FixClient>> clients;
  std::vector<bench::TrafficGenerator>         gens;

  for (const auto& sym : symbols)
  {
    clients.push_back(std::make_unique<fix::FixClient>("127.0.0.1", PORT_MULTI_SYMBOL));
    bench::TrafficConfig cfg;
    cfg.symbol = sym;
    cfg.mid_price = 500;
    cfg.spread = 10;
    cfg.cancel_rate = 0.5;
    gens.emplace_back(cfg);
  }

  for (auto _ : state)
  {
    std::vector<std::thread> threads;
    threads.reserve(symbols.size());
    for (size_t i = 0; i < symbols.size(); ++i)
    {
      threads.emplace_back([&, i]() { clients[i]->send(gens[i].next(LOB::Side::BUY)); });
    }
    for (auto& t : threads)
      t.join();
  }

  state.SetItemsProcessed(state.iterations() * symbols.size());
}
BENCHMARK(BM_EndToEnd_MultiSymbol)->Iterations(5000);

// Tick-to-trade: time from fill receipt (the "tick") to order acknowledgment.
// Models a market maker that re-quotes immediately on every fill.
// Chain measured: aggressor send → match → fill delivered to passive → re-quote sent → ack received.
static void BM_TickToTrade(benchmark::State& state)
{
  ServerFixture srv(PORT_TICK_TO_TRADE);

  std::atomic<bool>            ack_received{false};
  std::atomic<uint64_t>        uid{6'000'000};
  std::atomic<fix::FixClient*> passive_ptr{nullptr};

  // Passive client: receives fill (tick), immediately re-quotes.
  // Fills are identified by "31=" (LastPx), which only appears in execution reports.
  // Acks are identified by "39=0". Two separate ifs (not else-if) so both fire
  // if a single ::read() returns multiple messages in one buffer.
  auto passive = std::make_unique<fix::FixClient>(
      "127.0.0.1", PORT_TICK_TO_TRADE,
      [&](std::string_view msg)
      {
        if (msg.find("31=") != std::string_view::npos)
        {
          // Fill received — this is the tick. Re-quote immediately.
          fix::FixClient* p = passive_ptr.load(std::memory_order_acquire);
          if (p)
            p->send(fix::FixMessageBuilder::limit<LOB::Side::SELL>(
                uid.fetch_add(1, std::memory_order_relaxed), 1, 100, "MSFT"));
        }
        if (msg.find("39=0") != std::string_view::npos)
        {
          // Re-quote ack received — chain complete.
          ack_received.store(true, std::memory_order_release);
        }
      });
  passive_ptr.store(passive.get(), std::memory_order_release);

  fix::FixClient active("127.0.0.1", PORT_TICK_TO_TRADE, [](std::string_view) {});

  for (auto _ : state)
  {
    // Setup: place a resting sell (outside timed section).
    state.PauseTiming();
    ack_received.store(false, std::memory_order_relaxed);
    passive->send(fix::FixMessageBuilder::limit<LOB::Side::SELL>(
        uid.fetch_add(1, std::memory_order_relaxed), 1, 100, "MSFT"));
    while (!ack_received.load(std::memory_order_acquire))
      ;
    ack_received.store(false, std::memory_order_relaxed);
    state.ResumeTiming();

    // Timed: aggressor crosses → passive gets fill → re-quotes → ack.
    active.send(fix::FixMessageBuilder::market<LOB::Side::BUY>(
        uid.fetch_add(1, std::memory_order_relaxed), 1, "MSFT"));

    while (!ack_received.load(std::memory_order_acquire))
      ;
  }

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_TickToTrade)
    ->Repetitions(200)
    ->Iterations(20)
    ->ReportAggregatesOnly(true)
    ->ComputeStatistics("p50",
                        [](const std::vector<double>& v)
                        {
                          auto c = v;
                          std::sort(c.begin(), c.end());
                          return c[c.size() * 50 / 100];
                        })
    ->ComputeStatistics("p99",
                        [](const std::vector<double>& v)
                        {
                          auto c = v;
                          std::sort(c.begin(), c.end());
                          return c[c.size() * 99 / 100];
                        })
    ->ComputeStatistics("p999",
                        [](const std::vector<double>& v)
                        {
                          auto c = v;
                          std::sort(c.begin(), c.end());
                          return c[c.size() * 999 / 1000];
                        });

BENCHMARK_MAIN();
