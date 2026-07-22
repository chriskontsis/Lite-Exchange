// Round-trip latency breakdown.
//
// Splits the order-to-fill round trip into three segments using cross-core
// rdtscp stamps (valid here: constant_tsc + nonstop_tsc, kernel clocksource=tsc):
//
//   t0  bench : before push(inbound)
//   t1  shard : after  pop(inbound)      -> A = inbound transit + poll lag
//   t2  shard : after  add_order()       -> B = match compute
//   t3  shard : before push(outbound)
//   t4  bench : after  pop(outbound)     -> C = outbound transit + poll lag
//
// The shard writes t1/t2/t3 into the StampedFill; the outbound push (release)
// / pop (acquire) publishes them to the bench with no extra synchronisation.
//
// Each iteration first rests a sell and waits for its ack, so the queue is
// empty and the book primed before the timed buy — segment A then measures a
// clean transit, not queueing behind the resting order.

#include <hdr/hdr_histogram.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <thread>

#include "lx/book/order_book.hpp"
#include "lx/queue/spsc.hpp"
#include "lx/util/affinity.hpp"
#include "lx/util/tsc.hpp"

using Book = lx::book::OrderBook<65536, 2048>;

struct StampedFill
{
  lx::proto::Fill fill;
  uint64_t        t1;  // shard: after pop(inbound)
  uint64_t        t2;  // shard: after add_order
  uint64_t        t3;  // shard: before push(outbound)
};

static lx::proto::NewOrder make_order(uint64_t oid, lx::proto::Side side)
{
  lx::proto::NewOrder m{};
  m.hdr = {sizeof(m), lx::proto::MsgType::NEW_ORDER, 0};
  m.order_id = oid;
  m.price = 1000;
  m.qty = 1;
  m.side = side;
  m.tif = lx::proto::TimeInForce::GTC;
  return m;
}

int main()
{
  constexpr int WARMUP = 10'000;
  constexpr int MEASURE = 200'000;
  constexpr int TOTAL = WARMUP + MEASURE;

  lx::SpscQueue<lx::proto::NewOrder, 4096> inbound;
  lx::SpscQueue<StampedFill, 4096>         outbound;
  std::atomic<bool>                        running{true};

  std::thread shard(
      [&]
      {
        lx::util::pin_current_thread(2);
        Book            book{900, 1};
        lx::proto::Fill fills[64];
        while (running.load(std::memory_order_relaxed))
        {
          lx::proto::NewOrder msg;
          if (!inbound.pop(msg))
            continue;
          uint64_t t1 = lx::util::rdtscp();
          uint32_t fc = 0;
          book.add_order(msg, fills, fc, 64);
          uint64_t    t2 = lx::util::rdtscp();
          StampedFill sf{};
          if (fc > 0)
            sf.fill = fills[0];
          sf.t1 = t1;
          sf.t2 = t2;
          sf.t3 = lx::util::rdtscp();
          while (!outbound.push(sf))
            ;
        }
      });

  lx::util::pin_current_thread(3);
  uint64_t hz = lx::util::tsc_hz();
  std::printf("TSC frequency: %.3f GHz\n", hz / 1e9);

  hdr_histogram *h_in, *h_match, *h_out, *h_total;
  hdr_init(1, 10'000'000, 3, &h_in);
  hdr_init(1, 10'000'000, 3, &h_match);
  hdr_init(1, 10'000'000, 3, &h_out);
  hdr_init(1, 10'000'000, 3, &h_total);

  StampedFill sf{};
  for (int i = 0; i < TOTAL; ++i)
  {
    // Rest a sell and wait for its ack so the book is primed and queue empty.
    while (!inbound.push(make_order(i, lx::proto::Side::SELL)))
      ;
    while (!outbound.pop(sf))
      ;

    // Timed aggressor buy.
    uint64_t t0 = lx::util::rdtscp();
    while (!inbound.push(make_order(TOTAL + i, lx::proto::Side::BUY)))
      ;
    while (!outbound.pop(sf))
      ;
    uint64_t t4 = lx::util::rdtscp();

    if (i >= WARMUP)
    {
      hdr_record_value(h_in, static_cast<int64_t>(lx::util::tsc_to_ns(sf.t1 - t0, hz)));
      hdr_record_value(h_match, static_cast<int64_t>(lx::util::tsc_to_ns(sf.t2 - sf.t1, hz)));
      hdr_record_value(h_out, static_cast<int64_t>(lx::util::tsc_to_ns(t4 - sf.t3, hz)));
      hdr_record_value(h_total, static_cast<int64_t>(lx::util::tsc_to_ns(t4 - t0, hz)));
    }
  }

  running.store(false);
  shard.join();

  auto report = [](const char* name, hdr_histogram* h)
  {
    std::printf("%-22s p50=%5ld  p99=%5ld  p99.9=%6ld\n", name,
                hdr_value_at_percentile(h, 50.0), hdr_value_at_percentile(h, 99.0),
                hdr_value_at_percentile(h, 99.9));
  };
  std::printf("--- round-trip breakdown (ns) ---\n");
  report("A inbound transit", h_in);
  report("B match compute", h_match);
  report("C outbound transit", h_out);
  report("total", h_total);
  return 0;
}
