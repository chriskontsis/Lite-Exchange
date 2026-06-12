#include <gtest/gtest.h>

#include "lx/book/order.hpp"
#include "lx/book/pool.hpp"
#include "lx/book/price_level.hpp"

using namespace lx::book;

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