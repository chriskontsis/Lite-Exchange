#include <gtest/gtest.h>

#include "lx/proto/codec.hpp"
#include "lx/proto/messages.hpp"

using namespace lx::proto;

TEST(Messages, Sizes)
{
  static_assert(sizeof(Header) == 4);
  static_assert(sizeof(Logon) == 16);
  static_assert(sizeof(NewOrder) == 32);
  static_assert(sizeof(CancelOrder) == 16);
  static_assert(sizeof(Ack) == 16);
  static_assert(sizeof(Reject) == 16);
  static_assert(sizeof(Fill) == 32);
}

TEST(Messages, TriviallyCopiable)
{
  static_assert(std::is_trivially_copyable_v<NewOrder>);
  static_assert(std::is_trivially_copyable_v<Fill>);
}

TEST(Messages, HeaderFieldsRoundTrip)
{
  Header h{};
  h.len = 32;
  h.type = MsgType::NEW_ORDER;
  h.flags = 0x00;
  EXPECT_EQ(h.len, 32);
  EXPECT_EQ(h.type, MsgType::NEW_ORDER);
  EXPECT_EQ(h.flags, 0x00);
}

TEST(Messages, NewOrderRoundTrip)
{
  NewOrder msg{};
  msg.hdr      = {sizeof(NewOrder), MsgType::NEW_ORDER, 0};
  msg.order_id = 42;
  msg.symbol_id = 1;
  msg.price    = 10050;
  msg.qty      = 100;
  msg.side     = Side::BUY;
  msg.tif      = TimeInForce::GTC;
  EXPECT_EQ(msg.order_id, uint64_t{42});
  EXPECT_EQ(msg.price,    int64_t{10050});
  EXPECT_EQ(msg.side,     Side::BUY);
}

TEST(Codec, NeedMoreOnShortBuffer)
{
  std::byte buf[2]{};
  MsgType   type{};
  uint16_t  len{};
  EXPECT_EQ(tryDecode(buf, 2, type, len), DecodeResult::NEED_MORE);
}

TEST(Codec, NeedMoreOnIncompleteBody)
{
  NewOrder msg{};
  msg.hdr = {sizeof(NewOrder), MsgType::NEW_ORDER, 0};
  const auto* buf = reinterpret_cast<const std::byte*>(&msg);
  MsgType     type{};
  uint16_t    len{};

  EXPECT_EQ(tryDecode(buf, 8, type, len), DecodeResult::NEED_MORE);
}

TEST(Codec, InvalidOnBadLength)
{
  Header      hdr{1, MsgType::NEW_ORDER, 0};
  const auto* buf = reinterpret_cast<const std::byte*>(&hdr);
  MsgType     type{};
  uint16_t    len{};
  EXPECT_EQ(tryDecode(buf, sizeof(hdr), type, len), DecodeResult::INVALID);
}

TEST(Codec, OkOnCompleteNewOrder)
{
  NewOrder msg{};
  msg.hdr = {sizeof(NewOrder), MsgType::NEW_ORDER, 0};
  msg.order_id = 99;
  msg.price = 0;
  msg.qty = 10;
  msg.side = Side::BUY;
  msg.tif = TimeInForce::GTC;
  const auto* buf = reinterpret_cast<const std::byte*>(&msg);

  MsgType  type{};
  uint16_t len{};

  EXPECT_EQ(tryDecode(buf, sizeof(msg), type, len), DecodeResult::OK);
  EXPECT_EQ(type, MsgType::NEW_ORDER);
  EXPECT_EQ(len, sizeof(NewOrder));

  const auto* decoded = reinterpret_cast<const NewOrder*>(buf);
  EXPECT_EQ(decoded->order_id, uint64_t{99});
  EXPECT_EQ(decoded->qty,      uint32_t{10});
}