#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <thread>

#include "lx/engine/shard.hpp"
#include "lx/net/uring_gateway.hpp"

using namespace lx;
using namespace lx::proto;

using TestShard = engine::Shard<256, 64, 64>;

static NewOrder make_order(uint64_t oid, int64_t price, uint32_t qty, Side side)
{
  NewOrder m{};
  m.hdr = {sizeof(NewOrder), MsgType::NEW_ORDER, 0};
  m.order_id = oid;
  m.price = price;
  m.qty = qty;
  m.side = side;
  m.tif = TimeInForce::GTC;
  return m;
}

static int connect_client(uint16_t port)
{
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  EXPECT_GE(fd, 0);

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
  EXPECT_EQ(::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);

  timeval tv{.tv_sec = 2, .tv_usec = 0};
  ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  return fd;
}

TEST(UringGateway, NewOrderReturnsAck)
{
  TestShard                     shard{100, 1};
  net::UringGateway<TestShard>  gw{shard, /*port*/ 0};
  uint16_t                      port = gw.port();
  ASSERT_NE(port, 0);

  std::thread shard_thread([&] { shard.run(); });
  std::thread gw_thread([&] { gw.run(); });

  int      fd = connect_client(port);
  NewOrder order = make_order(1, 105, 50, Side::BUY);
  ASSERT_EQ(::send(fd, &order, sizeof(order), 0), static_cast<ssize_t>(sizeof(order)));

  Ack     ack{};
  ssize_t n = ::recv(fd, &ack, sizeof(ack), MSG_WAITALL);
  EXPECT_EQ(n, static_cast<ssize_t>(sizeof(Ack)));
  EXPECT_EQ(ack.hdr.type, MsgType::ACK);
  EXPECT_EQ(ack.order_id, uint64_t{1});
  EXPECT_NE(ack.order_token, uint64_t{0});

  ::close(fd);
  gw.stop();
  shard.stop();
  gw_thread.join();
  shard_thread.join();
}
