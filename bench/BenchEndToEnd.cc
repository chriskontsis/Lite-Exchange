#include <benchmark/benchmark.h>
#include <thread>
#include <atomic>
#include <chrono>
#include "../src/server/FixServer.hpp"
#include "../src/engine/EngineDispatcher.hpp"
#include "../src/ipc/MPSC_Queue.hpp"
#include "../src/ipc/OrderEvent.hpp"
#include "../src/ipc/FillEvent.hpp"
#include "../src/gateway/SessionRegistry.hpp"
#include "../src/fix/FixMessageBuilder.hpp"
#include "../src/client/FixClient.hpp"
#include "../src/net/EventLoop.hpp"
#include "../src/net/IoHandler.hpp"

static constexpr short PORT = 12399;

struct ServerFixture
{
    MPSC_Queue<ipc::OrderEvent, 4096> inputQ;
    MPSC_Queue<ipc::FillEvent, 4096>  outputQ;
    gateway::SessionRegistry          registry;
    fix::EngineDispatcher             dispatcher{inputQ, outputQ};
    net::EventLoop                    loop;
    fix::FixServer                    server{loop, PORT, inputQ, registry};
    std::atomic<bool>                 server_running{true};
    std::thread                       server_thread;
    std::atomic<bool>                 drain_running{true};
    std::atomic<uint64_t>             fills_received{0};
    std::thread                       drain_thread;

    ServerFixture()
    {
        server_thread = std::thread([this]() {
            net::Event events[64];
            while (server_running.load(std::memory_order_relaxed)) {
                int n = loop.wait(events, 64, /*timeout_ms=*/1);
                for (int i = 0; i < n; ++i) {
                    auto* handler = static_cast<net::IoHandler*>(events[i].ctx);
                    if (events[i].readable) handler->onReadable();
                    if (events[i].writable) handler->onWritable();
                    if (handler->wantsClose()) server.closeSession(events[i].fd);
                }
            }
        });
        drain_thread = std::thread([this]() {
            ipc::FillEvent fe;
            while (drain_running.load(std::memory_order_relaxed)) {
                if (outputQ.tryConsume(fe)) {
                    fills_received.fetch_add(1, std::memory_order_relaxed);
                    if (auto* session = registry.lookup(fe.session_id_))
                        session->sendData(fix::FixMessageBuilder::executionReport(fe));
                }
            }
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    ~ServerFixture()
    {
        server_running.store(false);
        drain_running.store(false);
        server_thread.join();
        drain_thread.join();
    }
};

// Scenario 1: limit order send rate (no matches)
static void BM_EndToEnd_LimitFlood(benchmark::State &state)
{
    ServerFixture srv;
    fix::FixClient client("127.0.0.1", PORT);
    std::string msg = fix::FixMessageBuilder::limit<LOB::Side::BUY>(1, 1,
                                                                    100, "AAPL");

    for (auto _ : state)
        client.send(msg);

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_EndToEnd_LimitFlood)->Iterations(10000);

// Scenario 2: matched pairs — measures full round trip including fill delivery 
static void BM_EndToEnd_MatchedPairs(benchmark::State &state)
{
    ServerFixture srv;
    fix::FixClient c1("127.0.0.1", PORT);
    fix::FixClient c2("127.0.0.1", PORT);

    LOB::UID uid = 1;
    for (auto _ : state)
    {
        srv.fills_received.store(0, std::memory_order_relaxed);
        c1.send(fix::FixMessageBuilder::limit<LOB::Side::BUY>(uid++, 1,
                                                              100, "MSFT"));
        c2.send(fix::FixMessageBuilder::limit<LOB::Side::SELL>(uid++, 1,
                                                               100, "MSFT"));

        // wait for both fill events (aggressor + resting)
        while (srv.fills_received.load(std::memory_order_relaxed) < 2)
            ;
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_EndToEnd_MatchedPairs)->Iterations(1000);

// Aggregate throughput — N persistent connections, round-robin sends.
// Persistent connections avoid TCP handshake cost; round-robin sends from each
// client per iteration exercises the MPSC queue with N concurrent producers.
static void BM_EndToEnd_MultiClientFlood(benchmark::State& state)
{
    ServerFixture srv;
    const int num_clients = state.range(0);

    std::vector<std::unique_ptr<fix::FixClient>> clients;
    clients.reserve(num_clients);
    for (int i = 0; i < num_clients; ++i)
        clients.push_back(std::make_unique<fix::FixClient>("127.0.0.1", PORT));

    LOB::UID uid = 1;
    for (auto _ : state) {
        std::vector<std::thread> threads;
        threads.reserve(num_clients);
        for (int i = 0; i < num_clients; ++i) {
            threads.emplace_back([&, i]() {
                clients[i]->send(
                    fix::FixMessageBuilder::limit<LOB::Side::BUY>(uid + i, 1, 100, "AAPL"));
            });
        }
        for (auto& t : threads) t.join();
        uid += num_clients;
    }

    state.SetItemsProcessed(state.iterations() * num_clients);
}
BENCHMARK(BM_EndToEnd_MultiClientFlood)->Arg(2)->Arg(4)->Arg(8)->Iterations(2000);

// Peak fill throughput — N buy + N sell clients, all crossing at the same price.
// Every order matches immediately. Measures max fills/sec the system can sustain.
static void BM_EndToEnd_FillThroughput(benchmark::State& state)
{
    ServerFixture srv;
    const int num_pairs = state.range(0);

    std::atomic<uint64_t> fills{0};
    auto on_fill = [&](std::string_view) {
        fills.fetch_add(1, std::memory_order_relaxed);
    };

    std::vector<std::unique_ptr<fix::FixClient>> buyers, sellers;
    for (int i = 0; i < num_pairs; ++i) {
        buyers.push_back(std::make_unique<fix::FixClient>("127.0.0.1", PORT, on_fill));
        sellers.push_back(std::make_unique<fix::FixClient>("127.0.0.1", PORT, on_fill));
    }

    LOB::UID uid{1};
    for (auto _ : state) {
        uint64_t expected = fills.load(std::memory_order_relaxed) + num_pairs * 2;
        for (int i = 0; i < num_pairs; ++i) {
            buyers[i]->send(fix::FixMessageBuilder::limit<LOB::Side::BUY>(uid++, 1, 100, "MSFT"));
            sellers[i]->send(fix::FixMessageBuilder::limit<LOB::Side::SELL>(uid++, 1, 100, "MSFT"));
        }
        while (fills.load(std::memory_order_relaxed) < expected) ;
    }

    state.SetItemsProcessed(fills.load());
}
BENCHMARK(BM_EndToEnd_FillThroughput)->Arg(1)->Arg(2)->Arg(4)->Iterations(500);

// Matched pairs with latency distribution — shows p50/p99/p999 tail latency.
// The most interview-relevant number: full tick-to-fill round-trip percentiles.
static void BM_EndToEnd_MatchedPairs_Latency(benchmark::State& state)
{
    ServerFixture srv;
    fix::FixClient c1("127.0.0.1", PORT);
    fix::FixClient c2("127.0.0.1", PORT);

    LOB::UID uid = 1;
    for (auto _ : state) {
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
    ->ComputeStatistics("p50",  [](const std::vector<double>& v) {
        auto c = v; std::sort(c.begin(), c.end());
        return c[c.size() * 50 / 100];
    })
    ->ComputeStatistics("p99",  [](const std::vector<double>& v) {
        auto c = v; std::sort(c.begin(), c.end());
        return c[c.size() * 99 / 100];
    })
    ->ComputeStatistics("p999", [](const std::vector<double>& v) {
        auto c = v; std::sort(c.begin(), c.end());
        return c[c.size() * 999 / 1000];
    });

BENCHMARK_MAIN();