#include <gtest/gtest.h>

#include "lx/engine/shard.hpp"

using namespace lx;
using namespace lx::proto;

static NewOrder make_order(uint64_t oid, int64_t price, uint32_t qty, Side side,
                           TimeInForce tif = TimeInForce::GTC)
{
  NewOrder msg{};
  msg.hdr = {sizeof(NewOrder), MsgType::NEW_ORDER, 0};
  msg.order_id = oid;
  msg.price = price;
  msg.qty = qty;
  msg.side = side;
  msg.tif = tif;
  return msg;
}

TEST(Shard, PassiveOrderRests)
{
  engine::Shard<256, 64, 64> shard{100, 1};
  shard.inbound().push(make_order(1, 105, 50, Side::BUY));
  shard.tick();

  Fill fill{};
  EXPECT_FALSE(shard.outbound().pop(fill));
  EXPECT_EQ(shard.best_bid(), int64_t{105});
}

TEST(Shard, MatchProducesFill)
{
  engine::Shard<256, 64, 64> shard{100, 1};
  shard.inbound().push(make_order(1, 105, 50, Side::SELL));
  shard.tick();

  shard.inbound().push(make_order(2, 105, 50, Side::BUY));
  shard.tick();

  Fill fill{};
  EXPECT_TRUE(shard.outbound().pop(fill));
  EXPECT_EQ(fill.qty, uint32_t{50});
  EXPECT_EQ(fill.aggressor_id, uint64_t{2});
  EXPECT_EQ(fill.resting_id, uint64_t{1});
  EXPECT_EQ(fill.price, int64_t{105});
  EXPECT_EQ(shard.best_ask(), INT64_MAX);
}

TEST(Shard, PartialMatchLeavesRemainder)
{
  engine::Shard<256, 64, 64> shard{100, 1};

  shard.inbound().push(make_order(1, 105, 100, Side::SELL));
  shard.tick();

  shard.inbound().push(make_order(2, 105, 30, Side::BUY));
  shard.tick();

  Fill fill{};
  EXPECT_TRUE(shard.outbound().pop(fill));
  EXPECT_EQ(fill.qty, uint32_t{30});
  EXPECT_EQ(shard.best_ask(), int64_t{105});
}

TEST(Shard, MultipleOrdersSameTick)
{
  engine::Shard<256, 64, 64> shard{100, 1};

  shard.inbound().push(make_order(1, 105, 50, Side::SELL));
  shard.inbound().push(make_order(2, 105, 50, Side::BUY));
  shard.tick();
  shard.tick();

  Fill fill{};
  EXPECT_TRUE(shard.outbound().pop(fill));
  EXPECT_EQ(fill.qty, uint32_t{50});
}

TEST(Shard, IocKilledWhenNoLiquidity)
{
  engine::Shard<256, 64, 64> shard{100, 1};

  shard.inbound().push(make_order(1, 105, 50, Side::BUY, TimeInForce::IOC));
  shard.tick();

  Fill fill{};
  EXPECT_FALSE(shard.outbound().pop(fill));
  EXPECT_EQ(shard.best_bid(), INT64_MIN);
}
