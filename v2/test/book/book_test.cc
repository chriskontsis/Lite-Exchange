#include <gtest/gtest.h>

#include "lx/book/order.hpp"
#include "lx/book/order_book.hpp"
#include "lx/book/pool.hpp"
#include "lx/book/price_level.hpp"

using namespace lx::book;
using namespace lx::proto;

TEST(Order, SizeIs32)
{
  static_assert(sizeof(Order) == 32);
}

TEST(Order, TriviallyCopiable)
{
  static_assert(std::is_trivially_copyable_v<Order>);
}

TEST(Pool, AllocReturnsValid)
{
  using P = Pool<Order, 8>;
  P        pool;
  uint32_t idx = pool.alloc();
  EXPECT_NE(idx, P::INVALID_IDX);
  EXPECT_LT(idx, 8u);
}

TEST(Pool, AllocAndFree)
{
  Pool<Order, 8> pool;
  uint32_t       idx = pool.alloc();
  pool.free(idx);
  uint32_t idx2 = pool.alloc();
  EXPECT_EQ(idx, idx2);
}

TEST(Pool, FullReturnsInvalid)
{
  using P = Pool<Order, 4>;
  P pool;
  for (int i = 0; i < 4; ++i)
    pool.alloc();
  EXPECT_EQ(pool.alloc(), P::INVALID_IDX);
}

TEST(Pool, SlotWriteRead)
{
  Pool<Order, 8> pool;
  uint32_t       idx = pool.alloc();
  pool[idx].order_id = 99;
  pool[idx].qty = 50;
  EXPECT_EQ(pool[idx].order_id, uint64_t{99});
  EXPECT_EQ(pool[idx].qty, uint64_t{50});
}

TEST(PriceLevel, PushIncreasesTotalQty)
{
  Pool<Order, 8> pool;
  PriceLevel     level;

  uint32_t a = pool.alloc();
  pool[a].qty = 100;
  pool[a].order_id = 1;
  level.push_back(a, pool.data());

  uint32_t b = pool.alloc();
  pool[b].qty = 200;
  pool[b].order_id = 2;
  level.push_back(b, pool.data());

  EXPECT_EQ(level.order_count, 2u);
  EXPECT_EQ(level.total_qty, 300u);
}

TEST(PriceLevel, PopFrontFIFO)
{
  Pool<Order, 8> pool;
  PriceLevel     level;

  uint32_t a = pool.alloc();
  pool[a].order_id = 1;
  pool[a].qty = 10;
  uint32_t b = pool.alloc();
  pool[b].order_id = 2;
  pool[b].qty = 20;
  level.push_back(a, pool.data());
  level.push_back(b, pool.data());

  EXPECT_EQ(level.pop_front(pool.data()), a);
  EXPECT_EQ(level.pop_front(pool.data()), b);
  EXPECT_TRUE(level.empty());
}

TEST(PriceLevel, RemoveMiddle)
{
  Pool<Order, 8> pool;
  PriceLevel     level;

  uint32_t a = pool.alloc();
  pool[a].qty = 10;
  uint32_t b = pool.alloc();
  pool[b].qty = 20;
  uint32_t c = pool.alloc();
  pool[c].qty = 30;
  level.push_back(a, pool.data());
  level.push_back(b, pool.data());
  level.push_back(c, pool.data());

  level.remove(b, pool.data());
  EXPECT_EQ(level.order_count, 2u);
  EXPECT_EQ(level.total_qty, 40u);
  EXPECT_EQ(level.pop_front(pool.data()), a);
  EXPECT_EQ(level.pop_front(pool.data()), c);
}

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

TEST(OrderBook, PassiveBuyPlaced)
{
  OrderBook<64, 1024> book{100, 1};
  Fill                fills[16];
  uint32_t            fc = 0;

  EXPECT_NE(book.add_order(make_order(1, 105, 100, Side::BUY), fills, fc), NULL_IDX);
  EXPECT_EQ(fc, 0u);
  EXPECT_EQ(book.best_bid_price(), int64_t{105});
  EXPECT_EQ(book.best_ask_price(), INT64_MAX);
}

TEST(OrderBook, PassiveSellPlaced)
{
  OrderBook<64, 1024> book{100, 1};
  Fill                fills[16];
  uint32_t            fc = 0;

  EXPECT_NE(book.add_order(make_order(1, 105, 100, Side::SELL), fills, fc), NULL_IDX);
  EXPECT_EQ(fc, 0u);
  EXPECT_EQ(book.best_ask_price(), int64_t{105});
  EXPECT_EQ(book.best_bid_price(), INT64_MAX);
}

TEST(OrderBook, FullMatch)
{
  OrderBook<64, 1024> book{100, 1};
  Fill                fills[16];
  uint32_t            fc = 0;
  book.add_order(make_order(1, 105, 50, Side::SELL), fills, fc);

  fc = 0;
  uint32_t slot = book.add_order(make_order(2, 105, 50, Side::BUY), fills, fc);
  EXPECT_EQ(slot, NULL_IDX);
  EXPECT_EQ(fc, 1u);
  EXPECT_EQ(fills[0].qty, uint32_t{50});
  EXPECT_EQ(fills[0].aggressor_id, uint64_t{2});
  EXPECT_EQ(fills[0].resting_id, uint64_t{1});
  EXPECT_EQ(fills[0].price, int64_t{105});
  EXPECT_EQ(book.best_ask_price(), INT64_MAX);
}

TEST(OrderBook, PartialMatch)
{
  OrderBook<64, 1024> book{100, 1};
  Fill                fills[16];
  uint32_t            fc = 0;
  book.add_order(make_order(1, 105, 30, Side::SELL), fills, fc);

  fc = 0;
  uint32_t slot = book.add_order(make_order(2, 105, 50, Side::BUY), fills, fc);
  EXPECT_NE(slot, NULL_IDX);
  EXPECT_EQ(fc, 1u);
  EXPECT_EQ(fills[0].qty, uint32_t{30});
  EXPECT_EQ(book.best_bid_price(), int64_t{105});
  EXPECT_EQ(book.best_ask_price(), INT64_MAX);
}

TEST(OrderBook, MultiLevelMatch)
{
  OrderBook<64, 1024> book{100, 1};
  Fill                fills[16];
  uint32_t            fc = 0;
  book.add_order(make_order(1, 105, 20, Side::SELL), fills, fc);
  book.add_order(make_order(2, 106, 30, Side::SELL), fills, fc);

  fc = 0;
  book.add_order(make_order(3, 106, 50, Side::BUY), fills, fc);
  EXPECT_EQ(fc, 2u);
  EXPECT_EQ(fills[0].qty, uint32_t{20});
  EXPECT_EQ(fills[1].qty, uint32_t{30});
  EXPECT_EQ(book.best_ask_price(), INT64_MAX);
}

TEST(OrderBook, CancelBid)
{
  OrderBook<64, 1024> book{100, 1};
  Fill                fills[16];
  uint32_t            fc = 0;
  uint32_t            slot = book.add_order(make_order(1, 105, 100, Side::BUY), fills, fc);
  EXPECT_NE(slot, NULL_IDX);
  book.cancel_order(slot);
  EXPECT_EQ(book.best_bid_price(), INT64_MIN);
}

TEST(OrderBook, IocKilled)
{
  OrderBook<64, 1024> book{100, 1};
  Fill                fills[16];
  uint32_t            fc = 0;
  uint32_t slot = book.add_order(make_order(1, 105, 100, Side::BUY, TimeInForce::IOC), fills, fc);
  EXPECT_EQ(slot, NULL_IDX);
  EXPECT_EQ(fc, 0u);
  EXPECT_EQ(book.best_bid_price(), INT64_MIN);
}