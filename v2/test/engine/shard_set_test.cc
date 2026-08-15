#include "lx/engine/shard_set.hpp"

#include <gtest/gtest.h>

#include "lx/engine/shard.hpp"

using namespace lx;
using namespace lx::proto;

static constexpr engine::ShardConfig TEST_SHARD{
    .max_orders = 256, .ladder_size = 64, .queue_depth = 64, .max_symbols = 2};
using TestShard = engine::Shard<TEST_SHARD>;

static InboundMsg make_order(uint16_t symbol, uint64_t order_id, int64_t price)
{
  InboundMsg msg{};
  msg.new_order.hdr = {sizeof(NewOrder), MsgType::NEW_ORDER, 0};
  msg.new_order.symbol_id = symbol;
  msg.new_order.order_id = order_id;
  msg.new_order.price = price;
  msg.new_order.qty = 10;
  msg.new_order.side = Side::BUY;
  msg.new_order.tif = TimeInForce::GTC;
  return msg;
}

TEST(ShardSet, RoutesInboundThroughRouter)
{
  TestShard                      s0{100, 1, 0};
  TestShard                      s1{100, 1, 1};
  engine::ShardSet<TestShard, 2> shards{{&s0, &s1}};

  ASSERT_TRUE(shards.router().push(make_order(0, 1, 105)));
  ASSERT_TRUE(shards.router().push(make_order(1, 2, 107)));
  s0.tick();
  s1.tick();

  EXPECT_EQ(s0.best_bid(/*symbol*/ 0), int64_t{105});
  EXPECT_EQ(s1.best_bid(/*symbol*/ 1), int64_t{107});
}

TEST(ShardSet, ExposesEachOutboundRing)
{
  TestShard                      s0{100, 1, 0};
  TestShard                      s1{100, 1, 1};
  engine::ShardSet<TestShard, 2> shards{{&s0, &s1}};

  ASSERT_TRUE(shards.router().push(make_order(0, 11, 105)));
  ASSERT_TRUE(shards.router().push(make_order(1, 22, 107)));
  s0.tick();
  s1.tick();

  OutboundMsg out0{};
  OutboundMsg out1{};
  ASSERT_TRUE(shards.outbound(0).pop(out0));
  ASSERT_TRUE(shards.outbound(1).pop(out1));
  EXPECT_EQ(out0.ack.order_id, uint64_t{11});
  EXPECT_EQ(out1.ack.order_id, uint64_t{22});
}
