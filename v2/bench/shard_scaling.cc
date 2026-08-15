// Engine-side shard scaling: does aggregate matching throughput scale with
// shard count? Drives ShardRouter directly — no sockets. Measured Phase 12:
// match compute is 53 ns, so one shard absorbs ~19M orders/sec while the epoll
// gateway peaks at 164k/sec. A socket-driven version would measure the reactor
// and print a flat line, hiding sharding entirely.
//
//   producer (core 0) -> N inbound rings -> N shards (isolated cores 2..)
//                     -> N outbound rings -> drainer (core 1)
//
// SPSC holds: one producer per inbound ring, one consumer per outbound ring.
//
// The producer counts failed pushes. A high stall rate means the rings were
// full and the SHARDS were the limit (what we want to measure); a low stall
// rate means the producer never saturated them and the number is producer-bound.
//
// Usage: shard_scaling [orders_per_run]

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <span>
#include <thread>
#include <vector>

#include "lx/engine/shard.hpp"
#include "lx/engine/shard_set.hpp"
#include "lx/util/affinity.hpp"

static constexpr uint16_t SYMBOL_COUNT = 32;  // global instruments, split across shards
static constexpr uint32_t MAX_ORDERS = 1 << 16;
static constexpr uint32_t LADDER_SIZE = 2048;
static constexpr uint32_t QUEUE_DEPTH = 8192;

static constexpr uint32_t PRODUCER_CORE = 0;
static constexpr uint32_t DRAINER_CORE = 1;
static constexpr uint32_t FIRST_SHARD_CORE = 2;

static constexpr int64_t  BASE_PRICE = 900;
static constexpr int64_t  TICK_SIZE = 1;
static constexpr int64_t  ORDER_PRICE = 1000;
static constexpr uint32_t ORDER_QTY = 1;

static constexpr uint64_t DEFAULT_ORDERS = 4'000'000;

// The router sends symbol s to shard s % N, so shard i hosts exactly those.
template <uint32_t N>
static constexpr lx::engine::ShardConfig SHARD_CFG{.max_orders = MAX_ORDERS,
                                                   .ladder_size = LADDER_SIZE,
                                                   .queue_depth = QUEUE_DEPTH,
                                                   .max_symbols = SYMBOL_COUNT / N,
                                                   .symbol_space = SYMBOL_COUNT};

struct Result
{
  double orders_per_sec;
  double stall_pct;
};

// One cycle rests a pass of sells then matches them with a pass of buys, so the
// arena reaches steady state instead of filling up with resting orders.
static std::vector<lx::proto::InboundMsg> build_cycle()
{
  // InboundMsg's default ctor is deleted (Header has a member initializer), so
  // the vector must be filled by push_back, never sized up front.
  std::vector<lx::proto::InboundMsg> cycle;
  cycle.reserve(SYMBOL_COUNT * 2);
  for (uint32_t i = 0; i < SYMBOL_COUNT * 2; ++i)
  {
    lx::proto::InboundMsg m{};
    m.new_order.hdr = {sizeof(lx::proto::NewOrder), lx::proto::MsgType::NEW_ORDER, 0};
    m.new_order.symbol_id = static_cast<uint16_t>(i % SYMBOL_COUNT);
    m.new_order.order_id = i;
    m.new_order.price = ORDER_PRICE;
    m.new_order.qty = ORDER_QTY;
    m.new_order.side = (i < SYMBOL_COUNT) ? lx::proto::Side::SELL : lx::proto::Side::BUY;
    m.new_order.tif = lx::proto::TimeInForce::GTC;
    cycle.push_back(m);
  }
  return cycle;
}

template <uint32_t N>
static Result run(uint64_t total_orders)
{
  using Shard = lx::engine::Shard<SHARD_CFG<N>>;
  using ShardSet = lx::engine::ShardSet<Shard, N>;

  std::vector<std::unique_ptr<Shard>> owned;
  std::array<Shard*, N>               ptrs{};
  for (uint32_t i = 0; i < N; ++i)
  {
    std::vector<uint16_t> symbols;
    for (uint16_t s = static_cast<uint16_t>(i); s < SYMBOL_COUNT; s = static_cast<uint16_t>(s + N))
      symbols.push_back(s);
    owned.push_back(std::make_unique<Shard>(BASE_PRICE, TICK_SIZE, static_cast<uint8_t>(i),
                                            std::span<const uint16_t>{symbols}));
    ptrs[i] = owned.back().get();
  }
  ShardSet shards{ptrs};

  std::vector<std::thread> shard_threads;
  for (uint32_t i = 0; i < N; ++i)
    shard_threads.emplace_back(
        [&, i]
        {
          lx::util::pin_current_thread(FIRST_SHARD_CORE + i);
          ptrs[i]->run();
        });

  std::atomic<bool> draining{true};
  std::thread       drainer(
      [&]
      {
        lx::util::pin_current_thread(DRAINER_CORE);
        lx::proto::OutboundMsg out{};
        while (draining.load(std::memory_order_relaxed))
          for (uint32_t s = 0; s < N; ++s)
            while (shards.outbound(s).pop(out))
              ;
      });

  const std::vector<lx::proto::InboundMsg> cycle = build_cycle();

  lx::util::pin_current_thread(PRODUCER_CORE);
  uint64_t stalls = 0;
  auto     t0 = std::chrono::steady_clock::now();
  for (uint64_t n = 0; n < total_orders; ++n)
  {
    const lx::proto::InboundMsg& msg = cycle[n % cycle.size()];
    while (!shards.router().push(msg))
      ++stalls;
  }
  auto t1 = std::chrono::steady_clock::now();

  for (auto& s : owned)
    s->stop();
  for (auto& t : shard_threads)
    t.join();
  draining.store(false, std::memory_order_relaxed);
  drainer.join();

  double secs = std::chrono::duration<double>(t1 - t0).count();
  return {static_cast<double>(total_orders) / secs,
          100.0 * static_cast<double>(stalls) / static_cast<double>(stalls + total_orders)};
}

static void report(uint32_t n, Result r, double baseline)
{
  std::printf("%7u %16.2f %11.2fx %13.1f%%\n", n, r.orders_per_sec / 1e6,
              r.orders_per_sec / baseline, r.stall_pct);
}

int main(int argc, char** argv)
{
  uint64_t orders = (argc > 1) ? std::strtoull(argv[1], nullptr, 10) : DEFAULT_ORDERS;

  std::printf("Engine-side shard scaling — %llu orders/run, %u instruments\n",
              static_cast<unsigned long long>(orders), SYMBOL_COUNT);
  std::printf("producer=core %u  drainer=core %u  shards=cores %u..\n\n", PRODUCER_CORE,
              DRAINER_CORE, FIRST_SHARD_CORE);
  std::printf("%7s %16s %12s %14s\n", "shards", "M orders/sec", "speedup", "producer stall");

  Result r1 = run<1>(orders);
  report(1, r1, r1.orders_per_sec);
  report(2, run<2>(orders), r1.orders_per_sec);
  return 0;
}
