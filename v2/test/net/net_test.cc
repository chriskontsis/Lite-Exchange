#include <gtest/gtest.h>

#include "lx/net/session.hpp"
#include "lx/proto/messages.hpp"
#include "lx/queue/spsc.hpp"

using namespace lx;
using namespace lx::proto;

static void write_frame(std::byte* dst, const NewOrder& msg)
{
  std::memcpy(dst, &msg, sizeof(msg));
}

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

TEST(Session, CompleteFrameDispatched)
{
  SpscQueue<NewOrder, 64> q;
  net::Session            s{/*fd*/ 0, /*session_id*/ 1};

  NewOrder  msg = make_order(1, 100, 10, Side::BUY);
  std::byte buf[sizeof(NewOrder)];
  write_frame(buf, msg);

  s.append(buf, sizeof(buf));
  uint32_t n = s.consume(q);

  EXPECT_EQ(n, 1u);
  NewOrder out{};
  EXPECT_TRUE(q.pop(out));
  EXPECT_EQ(out.order_id, uint64_t{1});
  EXPECT_EQ(out.price, int64_t{100});
}

TEST(Session, PartialFrameBuffered)
{
  SpscQueue<NewOrder, 64> q;
  net::Session            s{0, 1};

  NewOrder  msg = make_order(2, 200, 20, Side::SELL);
  std::byte buf[sizeof(NewOrder)];
  write_frame(buf, msg);

  // Send only first 10 bytes
  s.append(buf, 10);
  EXPECT_EQ(s.consume(q), 0u);
  EXPECT_FALSE(q.pop(msg));

  // Send the rest
  s.append(buf + 10, sizeof(buf) - 10);
  EXPECT_EQ(s.consume(q), 1u);
  NewOrder out{};
  EXPECT_TRUE(q.pop(out));
  EXPECT_EQ(out.order_id, uint64_t{2});
}

TEST(Session, TwoFramesInOneRecv)
{
  SpscQueue<NewOrder, 64> q;
  net::Session            s{0, 1};

  NewOrder  a = make_order(3, 100, 5, Side::BUY);
  NewOrder  b = make_order(4, 101, 10, Side::SELL);
  std::byte buf[sizeof(NewOrder) * 2];
  write_frame(buf, a);
  write_frame(buf + sizeof(NewOrder), b);

  s.append(buf, sizeof(buf));
  EXPECT_EQ(s.consume(q), 2u);

  NewOrder out{};
  q.pop(out);
  EXPECT_EQ(out.order_id, uint64_t{3});
  q.pop(out);
  EXPECT_EQ(out.order_id, uint64_t{4});
}

TEST(Session, InvalidFrameClosesSession)
{
  SpscQueue<NewOrder, 64> q;
  net::Session            s{0, 1};

  // Corrupt header: len = 1 (below MIN_FRAME_LEN threshold)
  std::byte buf[4]{};
  buf[0] = std::byte{1};  // len low byte = 1
  s.append(buf, 4);
  s.consume(q);

  EXPECT_TRUE(s.closed());
}