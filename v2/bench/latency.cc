#include <hdr/hdr_histogram.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>

#include "lx/engine/shard.hpp"
#include "lx/util/tsc.hpp"

using Shard = lx::engine::Shard<65536, 2048, 4096>;

static lx::proto::NewOrder make_sell(uint64_t oid)
{
  lx::proto::NewOrder msg{};
  msg.hdr = {sizeof(msg), lx::proto::MsgType::NEW_ORDER, 0};
  msg.order_id = oid;
  msg.price = 1000;
  msg.qty = 1;
  msg.side = lx::proto::Side::SELL;
  msg.tif = lx::proto::TimeInForce::GTC;
  return msg;
}

static lx::proto::NewOrder make_buy(uint64_t oid)
{
  lx::proto::NewOrder msg{};
  msg.hdr = {sizeof(msg), lx::proto::MsgType::NEW_ORDER, 0};
  msg.order_id = oid;
  msg.price = 1000;
  msg.qty = 1;
  msg.side = lx::proto::Side::BUY;
  msg.tif = lx::proto::TimeInForce::GTC;
  return msg;
}

int main()
{
  constexpr int WARMUP = 10'000;
  constexpr int MEASURE = 200'000;
  constexpr int TOTAL = WARMUP + MEASURE;

  Shard shard{900, 1};

  std::thread shard_thread([&] { shard.run(); });

  uint64_t hz = lx::util::tsc_hz();
  std::printf("TSC frequency: %.3f GHz\n", hz / 1e9);

  hdr_histogram* hist = nullptr;
  hdr_init(1, 10'000'000, 3, &hist);

  lx::proto::Fill fill{};

  // Each iteration: push one resting sell, wait for it to land in book,
  // then push a matching buy and measure round-trip to fill.
  for (int i = 0; i < TOTAL; ++i)
  {
    // Place resting sell
    while (!shard.inbound().push(make_sell(i)));

    // Stamp and send aggressor buy
    uint64_t t0 = lx::util::rdtscp();
    while (!shard.inbound().push(make_buy(TOTAL + i)));
    while (!shard.outbound().pop(fill));
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