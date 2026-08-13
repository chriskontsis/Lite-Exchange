// Order-to-ack latency through the io_uring gateway over loopback TCP. Same
// closed-loop harness as gateway_latency.cc; pass "sqpoll" to enable SQPOLL.

#include <arpa/inet.h>
#include <hdr/hdr_histogram.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>

#include "lx/engine/shard.hpp"
#include "lx/net/uring_gateway.hpp"
#include "lx/util/affinity.hpp"
#include "lx/util/tsc.hpp"

using Shard = lx::engine::Shard<65536, 2048, 4096>;

int main(int argc, char** argv)
{
  constexpr int WARMUP = 2000;
  constexpr int MEASURE = 40000;
  constexpr int TOTAL = WARMUP + MEASURE;  // < pool capacity: every order rests

  // Pass "sqpoll" to enable the kernel SQ poll thread (pinned to isolated core 7).
  bool sqpoll = (argc > 1 && std::strcmp(argv[1], "sqpoll") == 0);
  std::printf("mode: %s\n", sqpoll ? "SQPOLL" : "plain io_uring");

  Shard                        shard{900, 1};
  lx::net::UringGateway<Shard> gw{shard, /*port*/ 0, sqpoll, /*sq_cpu*/ 7};
  uint16_t                     port = gw.port();
  std::atomic<bool>       running{true};

  std::thread shard_thread(
      [&]
      {
        lx::util::pin_current_thread(2);
        shard.run();
      });
  std::thread gw_thread(
      [&]
      {
        lx::util::pin_current_thread(6);
        while (running.load(std::memory_order_relaxed))
          gw.poll_once(0);  // busy-poll: drains outbound every iteration
      });

  lx::util::pin_current_thread(3);

  int         fd = ::socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
  {
    std::perror("connect");
    return 1;
  }
  int one = 1;
  ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

  uint64_t hz = lx::util::tsc_hz();
  std::printf("TSC frequency: %.3f GHz\n", hz / 1e9);

  hdr_histogram* hist = nullptr;
  hdr_init(1, 10'000'000, 3, &hist);

  lx::proto::NewOrder order{};
  order.hdr = {sizeof(order), lx::proto::MsgType::NEW_ORDER, 0};
  order.qty = 1;
  order.side = lx::proto::Side::BUY;
  order.tif = lx::proto::TimeInForce::GTC;

  lx::proto::Ack ack{};
  for (int i = 0; i < TOTAL; ++i)
  {
    order.order_id = static_cast<uint64_t>(i) + 1;
    order.price = 901 + (i % 2000);  // stays inside the 2048-level ladder; rests

    uint64_t t0 = lx::util::rdtscp();
    ::send(fd, &order, sizeof(order), 0);

    std::size_t got = 0;
    while (got < sizeof(ack))
    {
      ssize_t r = ::recv(fd, reinterpret_cast<std::byte*>(&ack) + got, sizeof(ack) - got, 0);
      if (r <= 0)
        break;
      got += static_cast<std::size_t>(r);
    }
    uint64_t t1 = lx::util::rdtscp();

    if (i >= WARMUP)
      hdr_record_value(hist, static_cast<int64_t>(lx::util::tsc_to_ns(t1 - t0, hz)));
  }

  std::printf("--- order-to-ack over io_uring gateway (ns) ---\n");
  std::printf("p50:    %6ld ns\n", hdr_value_at_percentile(hist, 50.0));
  std::printf("p99:    %6ld ns\n", hdr_value_at_percentile(hist, 99.0));
  std::printf("p99.9:  %6ld ns\n", hdr_value_at_percentile(hist, 99.9));
  std::printf("p99.99: %6ld ns\n", hdr_value_at_percentile(hist, 99.99));
  std::printf("max:    %6ld ns\n", hdr_max(hist));

  running.store(false, std::memory_order_relaxed);
  ::close(fd);
  shard.stop();
  gw_thread.join();
  shard_thread.join();
  hdr_close(hist);
  return 0;
}
