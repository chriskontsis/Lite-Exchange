// Aggregate orders/sec through the gateway under many concurrent connections —
// the axis where io_uring's syscall batching beats epoll. Each loader bursts a
// pipeline of orders across all its connections then drains the acks, so many
// fds are readable at once. Usage: gateway_throughput [epoll|uring|sqpoll]

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

#include "lx/engine/shard.hpp"
#include "lx/engine/shard_set.hpp"
#include "lx/net/gateway.hpp"
#include "lx/net/uring_gateway.hpp"
#include "lx/util/affinity.hpp"

static constexpr lx::engine::ShardConfig SHARD_CFG{
    .max_orders = 1 << 20, .ladder_size = 2048, .queue_depth = 16384};
using Shard = lx::engine::Shard<SHARD_CFG>;
using ShardSet = lx::engine::ShardSet<Shard, 1>;

constexpr int      CONNS = 128;
constexpr int      LOADERS = 4;
constexpr int      PIPELINE = 8;
constexpr uint64_t TOTAL = 400'000;

static lx::proto::NewOrder make_order(uint64_t oid)
{
  lx::proto::NewOrder m{};
  m.hdr = {sizeof(m), lx::proto::MsgType::NEW_ORDER, 0};
  m.order_id = oid;
  m.price = 901 + (oid % 2000);  // stays inside the ladder; rests (no asks)
  m.qty = 1;
  m.side = lx::proto::Side::BUY;
  m.tif = lx::proto::TimeInForce::GTC;
  return m;
}

static int connect_client(uint16_t port)
{
  int         fd = ::socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
    return -1;
  int one = 1;
  ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  return fd;
}

// Returns aggregate throughput in orders/sec.
static double run_load(uint16_t port)
{
  constexpr int  per_loader_conns = CONNS / LOADERS;
  const uint64_t per_loader_orders = TOTAL / LOADERS;

  std::atomic<int>  ready{0};
  std::atomic<bool> go{false};

  std::vector<std::thread> loaders;
  for (int L = 0; L < LOADERS; ++L)
  {
    loaders.emplace_back(
        [&, L]
        {
          int fds[per_loader_conns];
          for (int c = 0; c < per_loader_conns; ++c)
            fds[c] = connect_client(port);

          ready.fetch_add(1, std::memory_order_release);
          while (!go.load(std::memory_order_acquire))
            ;

          uint64_t       oid = static_cast<uint64_t>(L) * 100'000'000ULL + 1;
          uint64_t       sent = 0;
          lx::proto::Ack ackbuf[PIPELINE];
          while (sent < per_loader_orders)
          {
            for (int c = 0; c < per_loader_conns; ++c)
              for (int k = 0; k < PIPELINE; ++k)
              {
                lx::proto::NewOrder o = make_order(oid++);
                ::send(fds[c], &o, sizeof(o), MSG_NOSIGNAL);
              }
            for (int c = 0; c < per_loader_conns; ++c)
            {
              std::size_t want = PIPELINE * sizeof(lx::proto::Ack);
              std::size_t got = 0;
              auto*       p = reinterpret_cast<std::byte*>(ackbuf);
              while (got < want)
              {
                ssize_t r = ::recv(fds[c], p + got, want - got, 0);
                if (r <= 0)
                  break;
                got += static_cast<std::size_t>(r);
              }
            }
            sent += static_cast<uint64_t>(per_loader_conns) * PIPELINE;
          }

          for (int c = 0; c < per_loader_conns; ++c)
            ::close(fds[c]);
        });
  }

  while (ready.load(std::memory_order_acquire) < LOADERS)
    ;
  auto t0 = std::chrono::steady_clock::now();
  go.store(true, std::memory_order_release);
  for (auto& t : loaders)
    t.join();
  auto t1 = std::chrono::steady_clock::now();

  double   secs = std::chrono::duration<double>(t1 - t0).count();
  uint64_t done = static_cast<uint64_t>(per_loader_orders) * LOADERS;
  return static_cast<double>(done) / secs;
}

template <typename GW>
static void measure(GW& gw, const char* mode)
{
  uint16_t          port = gw.port();
  std::atomic<bool> running{true};
  std::thread       gw_thread(
      [&]
      {
        lx::util::pin_current_thread(6);
        while (running.load(std::memory_order_relaxed))
          gw.poll_once(0);
      });

  double tput = run_load(port);
  std::printf("%-18s %10.0f orders/sec\n", mode, tput);

  running.store(false, std::memory_order_relaxed);
  gw_thread.join();
}

int main(int argc, char** argv)
{
  const char* mode = (argc > 1) ? argv[1] : "epoll";

  Shard       shard{900, 1};
  std::thread shard_thread(
      [&]
      {
        lx::util::pin_current_thread(2);
        shard.run();
      });

  if (std::strcmp(mode, "epoll") == 0)
  {
    ShardSet                   shards{{&shard}};
    lx::net::Gateway<ShardSet> gw{shards, 0};
    measure(gw, "epoll");
  }
  else if (std::strcmp(mode, "uring") == 0)
  {
    lx::net::UringGateway<Shard> gw{shard, 0, /*sqpoll*/ false};
    measure(gw, "io_uring");
  }
  else if (std::strcmp(mode, "sqpoll") == 0)
  {
    lx::net::UringGateway<Shard> gw{shard, 0, /*sqpoll*/ true, /*sq_cpu*/ 7};
    measure(gw, "io_uring+SQPOLL");
  }
  else
  {
    std::printf("unknown mode: %s (use epoll|uring|sqpoll)\n", mode);
  }

  shard.stop();
  shard_thread.join();
  return 0;
}
