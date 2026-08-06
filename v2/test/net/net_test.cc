#include <gtest/gtest.h>

#include "lx/net/session.hpp"
#include "lx/proto/messages.hpp"
#include "lx/queue/spsc.hpp"

using namespace lx;
using namespace lx::proto;

static NewOrder make_order(uint64_t oid, int64_t price, uint32_t qty, Side side)
{
  NewOrder msg{};
  msg.hdr = {sizeof(NewOrder), MsgType::NEW_ORDER, 0};
  msg.order_id = oid;
  msg.price = price;
  msg.qty = qty;
  msg.side = side;
  msg.tif = TimeInForce::GTC;
  return msg;
}

static CancelOrder make_cancel(uint64_t token)
{
  CancelOrder msg{};
  msg.hdr = {sizeof(CancelOrder), MsgType::CANCEL_ORDER, 0};
  msg.order_token = token;
  return msg;
}

TEST(Session, CompleteFrameDispatched)
{
  SpscQueue<InboundMsg, 64> q;
  net::Session              s{/*fd*/ 0, /*session_id*/ 1};

  NewOrder  msg = make_order(1, 100, 10, Side::BUY);
  std::byte buf[sizeof(NewOrder)];
  std::memcpy(buf, &msg, sizeof(msg));

  s.append(buf, sizeof(buf));
  uint32_t n = s.consume(q);

  EXPECT_EQ(n, 1u);
  InboundMsg out{};
  EXPECT_TRUE(q.pop(out));
  EXPECT_EQ(out.hdr.type, MsgType::NEW_ORDER);
  EXPECT_EQ(out.new_order.order_id, uint64_t{1});
  EXPECT_EQ(out.new_order.price, int64_t{100});
}

TEST(Session, CancelFrameDispatched)
{
  SpscQueue<InboundMsg, 64> q;
  net::Session              s{0, 1};

  CancelOrder msg = make_cancel(0xDEADBEEF12345678ULL);
  std::byte   buf[sizeof(CancelOrder)];
  std::memcpy(buf, &msg, sizeof(msg));

  s.append(buf, sizeof(buf));
  EXPECT_EQ(s.consume(q), 1u);

  InboundMsg out{};
  ASSERT_TRUE(q.pop(out));
  EXPECT_EQ(out.hdr.type, MsgType::CANCEL_ORDER);
  EXPECT_EQ(out.cancel.order_token, 0xDEADBEEF12345678ULL);
}

TEST(Session, PartialFrameBuffered)
{
  SpscQueue<InboundMsg, 64> q;
  net::Session              s{0, 1};

  NewOrder  msg = make_order(2, 200, 20, Side::SELL);
  std::byte buf[sizeof(NewOrder)];
  std::memcpy(buf, &msg, sizeof(msg));

  // Send only first 10 bytes
  s.append(buf, 10);
  EXPECT_EQ(s.consume(q), 0u);
  InboundMsg out{};
  EXPECT_FALSE(q.pop(out));

  // Send the rest
  s.append(buf + 10, sizeof(buf) - 10);
  EXPECT_EQ(s.consume(q), 1u);
  ASSERT_TRUE(q.pop(out));
  EXPECT_EQ(out.new_order.order_id, uint64_t{2});
}

TEST(Session, TwoFramesInOneRecv)
{
  SpscQueue<InboundMsg, 64> q;
  net::Session              s{0, 1};

  NewOrder  a = make_order(3, 100, 5, Side::BUY);
  NewOrder  b = make_order(4, 101, 10, Side::SELL);
  std::byte buf[sizeof(NewOrder) * 2];
  std::memcpy(buf, &a, sizeof(a));
  std::memcpy(buf + sizeof(NewOrder), &b, sizeof(b));

  s.append(buf, sizeof(buf));
  EXPECT_EQ(s.consume(q), 2u);

  InboundMsg out{};
  q.pop(out);
  EXPECT_EQ(out.new_order.order_id, uint64_t{3});
  q.pop(out);
  EXPECT_EQ(out.new_order.order_id, uint64_t{4});
}

TEST(Session, MixedNewAndCancelPreserveOrder)
{
  SpscQueue<InboundMsg, 64> q;
  net::Session              s{0, 1};

  NewOrder    a = make_order(5, 100, 5, Side::BUY);
  CancelOrder c = make_cancel(0xAABBCCDDULL);
  std::byte   buf[sizeof(NewOrder) + sizeof(CancelOrder)];
  std::memcpy(buf, &a, sizeof(a));
  std::memcpy(buf + sizeof(NewOrder), &c, sizeof(c));

  s.append(buf, sizeof(buf));
  EXPECT_EQ(s.consume(q), 2u);

  InboundMsg out{};
  ASSERT_TRUE(q.pop(out));
  EXPECT_EQ(out.hdr.type, MsgType::NEW_ORDER);
  ASSERT_TRUE(q.pop(out));
  EXPECT_EQ(out.hdr.type, MsgType::CANCEL_ORDER);
  EXPECT_EQ(out.cancel.order_token, 0xAABBCCDDULL);
}

TEST(Session, InvalidFrameClosesSession)
{
  SpscQueue<InboundMsg, 64> q;
  net::Session              s{0, 1};

  // Corrupt header: len = 1 (below MIN_FRAME_LEN threshold)
  std::byte buf[4]{};
  buf[0] = std::byte{1};  // len low byte = 1
  s.append(buf, 4);
  s.consume(q);

  EXPECT_TRUE(s.closed());
}
