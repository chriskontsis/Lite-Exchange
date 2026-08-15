#include <hdr/hdr_histogram.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>

#include "lx/engine/shard.hpp"
#include "lx/util/affinity.hpp"
#include "lx/util/tsc.hpp"

static constexpr lx::engine::ShardConfig SHARD_CFG{
    .max_orders = 65536, .ladder_size = 2048, .queue_depth = 4096};
using Shard = lx::engine::Shard<SHARD_CFG>;

static lx::proto::InboundMsg make_order(uint64_t oid, lx::proto::Side side)
{
  lx::proto::InboundMsg msg{};
  msg.new_order.hdr = {sizeof(lx::proto::NewOrder), lx::proto::MsgType::NEW_ORDER, 0};
  msg.new_order.order_id = oid;
  msg.new_order.price = 1000;
  msg.new_order.qty = 1;
  msg.new_order.side = side;
  msg.new_order.tif = lx::proto::TimeInForce::GTC;
  return msg;
}

static lx::proto::InboundMsg make_sell(uint64_t oid)
{
  return make_order(oid, lx::proto::Side::SELL);
}

static lx::proto::InboundMsg make_buy(uint64_t oid)
{
  return make_order(oid, lx::proto::Side::BUY);
}

int main()
{
  constexpr int WARMUP = 10'000;
  constexpr int MEASURE = 200'000;
  constexpr int TOTAL = WARMUP + MEASURE;

  Shard shard{900, 1};

  std::thread shard_thread(
      [&]
      {
        lx::util::pin_current_thread(2);
        shard.run();
      });
  lx::util::pin_current_thread(3);

  uint64_t hz = lx::util::tsc_hz();
  std::printf("TSC frequency: %.3f GHz\n", hz / 1e9);

  hdr_histogram* hist = nullptr;
  hdr_init(1, 10'000'000, 3, &hist);

  lx::proto::OutboundMsg out{};

  // Rest a sell and drain its Ack (untimed), then time a matching buy to Fill.
  for (int i = 0; i < TOTAL; ++i)
  {
    // Place resting sell, wait for its Ack so the book is primed.
    while (!shard.inbound().push(make_sell(i)))
      ;
    while (!shard.outbound().pop(out))
      ;

    // Stamp and send aggressor buy; the full match produces only a Fill.
    uint64_t t0 = lx::util::rdtscp();
    while (!shard.inbound().push(make_buy(TOTAL + i)))
      ;
    while (!shard.outbound().pop(out))
      ;
    uint64_t t1 = lx::util::rdtscp();

    if (i >= WARMUP)
      hdr_record_value(hist, static_cast<int64_t>(lx::util::tsc_to_ns(t1 - t0, hz)));
  }

  shard.stop();
  shard_thread.join();

  std::printf("--- order-to-fill latency (ns) ---\n");
  std::printf("p50:    %6ld ns\n", hdr_value_at_percentile(hist, 50.0));
  std::printf("p99:    %6ld ns\n", hdr_value_at_percentile(hist, 99.0));
  std::printf("p99.9:  %6ld ns\n", hdr_value_at_percentile(hist, 99.9));
  std::printf("p99.99: %6ld ns\n", hdr_value_at_percentile(hist, 99.99));
  std::printf("max:    %6ld ns\n", hdr_max(hist));

  hdr_close(hist);
  return 0;
}