#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <thread>

#include "lx/engine/shard.hpp"
#include "lx/net/gateway.hpp"

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

// Connect a client socket to 127.0.0.1:port with a small recv timeout so the
// test fails fast instead of hanging if nothing comes back.
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

// Drain Acks from fd until a short timeout elapses; return how many arrived.
static int count_acks(int fd, int budget_ms)
{
  timeval tv{.tv_sec = 0, .tv_usec = budget_ms * 1000};
  ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  int count = 0;
  Ack a{};
  while (::recv(fd, &a, sizeof(a), MSG_WAITALL) == static_cast<ssize_t>(sizeof(Ack)))
    ++count;
  return count;
}

TEST(Gateway, NewOrderReturnsAck)
{
  TestShard      shard{100, 1};
  net::Gateway<TestShard> gw{shard, /*port*/ 0};
  uint16_t       port = gw.port();
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

TEST(Gateway, OutboundRoutedToOriginatingClient)
{
  TestShard               shard{100, 1};
  net::Gateway<TestShard> gw{shard, /*port*/ 0};
  uint16_t                port = gw.port();
  ASSERT_NE(port, 0);

  std::thread shard_thread([&] { shard.run(); });
  std::thread gw_thread([&] { gw.run(); });

  int a = connect_client(port);
  int b = connect_client(port);

  // Two distinct resting orders from two clients; neither should match.
  NewOrder oa = make_order(1, 105, 50, Side::BUY);
  NewOrder ob = make_order(2, 104, 50, Side::BUY);
  ASSERT_EQ(::send(a, &oa, sizeof(oa), 0), static_cast<ssize_t>(sizeof(oa)));
  ASSERT_EQ(::send(b, &ob, sizeof(ob), 0), static_cast<ssize_t>(sizeof(ob)));

  // Each client must receive exactly its own Ack — not the other's.
  EXPECT_EQ(count_acks(a, 300), 1);
  EXPECT_EQ(count_acks(b, 300), 1);

  ::close(a);
  ::close(b);
  gw.stop();
  shard.stop();
  gw_thread.join();
  shard_thread.join();
}

TEST(Gateway, FillRoutedToBothParties)
{
  TestShard               shard{100, 1};
  net::Gateway<TestShard> gw{shard, /*port*/ 0};
  uint16_t                port = gw.port();
  ASSERT_NE(port, 0);

  std::thread shard_thread([&] { shard.run(); });
  std::thread gw_thread([&] { gw.run(); });

  int a = connect_client(port);
  int b = connect_client(port);

  // Client A rests a sell and reads its resting Ack.
  NewOrder sell = make_order(1, 105, 50, Side::SELL);
  ASSERT_EQ(::send(a, &sell, sizeof(sell), 0), static_cast<ssize_t>(sizeof(sell)));
  Ack ack{};
  ASSERT_EQ(::recv(a, &ack, sizeof(ack), MSG_WAITALL), static_cast<ssize_t>(sizeof(Ack)));
  ASSERT_EQ(ack.hdr.type, MsgType::ACK);

  // Client B aggresses, fully matching A's sell.
  NewOrder buy = make_order(2, 105, 50, Side::BUY);
  ASSERT_EQ(::send(b, &buy, sizeof(buy), 0), static_cast<ssize_t>(sizeof(buy)));

  // The aggressor (B) receives its fill.
  Fill bf{};
  EXPECT_EQ(::recv(b, &bf, sizeof(bf), MSG_WAITALL), static_cast<ssize_t>(sizeof(Fill)));
  EXPECT_EQ(bf.hdr.type, MsgType::FILL);

  // The passive owner (A) must ALSO receive a fill for the same trade.
  Fill af{};
  EXPECT_EQ(::recv(a, &af, sizeof(af), MSG_WAITALL), static_cast<ssize_t>(sizeof(Fill)));
  EXPECT_EQ(af.hdr.type, MsgType::FILL);
  EXPECT_EQ(af.resting_id, uint64_t{1});
  EXPECT_EQ(af.aggressor_id, uint64_t{2});

  ::close(a);
  ::close(b);
  gw.stop();
  shard.stop();
  gw_thread.join();
  shard_thread.join();
}

TEST(Gateway, CancelOverWireRemovesOrder)
{
  TestShard               shard{100, 1};
  net::Gateway<TestShard> gw{shard, /*port*/ 0};
  uint16_t                port = gw.port();
  ASSERT_NE(port, 0);

  std::thread shard_thread([&] { shard.run(); });
  std::thread gw_thread([&] { gw.run(); });

  int fd = connect_client(port);

  // Submit a resting order and read its Ack to learn the exchange token.
  NewOrder order = make_order(1, 105, 50, Side::BUY);
  ASSERT_EQ(::send(fd, &order, sizeof(order), 0), static_cast<ssize_t>(sizeof(order)));
  Ack ack{};
  ASSERT_EQ(::recv(fd, &ack, sizeof(ack), MSG_WAITALL), static_cast<ssize_t>(sizeof(Ack)));
  ASSERT_EQ(ack.hdr.type, MsgType::ACK);
  uint64_t token = ack.order_token;

  // Echo the token back in a cancel; expect a cancel Ack and an empty book.
  CancelOrder cxl{};
  cxl.hdr = {sizeof(CancelOrder), MsgType::CANCEL_ORDER, 0};
  cxl.order_token = token;
  ASSERT_EQ(::send(fd, &cxl, sizeof(cxl), 0), static_cast<ssize_t>(sizeof(cxl)));

  Ack cack{};
  ASSERT_EQ(::recv(fd, &cack, sizeof(cack), MSG_WAITALL), static_cast<ssize_t>(sizeof(Ack)));
  EXPECT_EQ(cack.hdr.type, MsgType::ACK);
  EXPECT_EQ(shard.best_bid(), INT64_MIN);

  ::close(fd);
  gw.stop();
  shard.stop();
  gw_thread.join();
  shard_thread.join();
}
