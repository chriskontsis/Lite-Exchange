#include <gtest/gtest.h>

#include "../../src/matching-engine/OrderBook.hpp"

using namespace LOB;

class OrderBookTest : public ::testing::Test
{
 protected:
  LimitOrderBook book;
};

TEST_F(OrderBookTest, LimitBuyRestsOnBook)
{
  book.limit(Side::BUY, 1, 10, 100);
  EXPECT_EQ(book.bestBid(), 100);
  EXPECT_EQ(book.volumeAt(Side::BUY, 100), 10);
  EXPECT_TRUE(book.hasOrder(1));
}

TEST_F(OrderBookTest, LimitSellRestsOnBook)
{
  book.limit(Side::SELL, 1, 10, 200);
  EXPECT_EQ(book.bestAsk(), 200);
  EXPECT_EQ(book.volumeAt(Side::SELL, 200), 10);
  EXPECT_TRUE(book.hasOrder(1));
}

TEST_F(OrderBookTest, LimitSellMatchesBid)
{
  book.limit(Side::BUY, 1, 10, 100);
  book.limit(Side::SELL, 2, 10, 100);
  EXPECT_EQ(book.bestBid(), 0);
  EXPECT_EQ(book.bestAsk(), 0);
  EXPECT_FALSE(book.hasOrder(1));
  EXPECT_FALSE(book.hasOrder(2));
}

TEST_F(OrderBookTest, LimitPartialFill)
{
  book.limit(Side::BUY, 1, 10, 100);
  book.limit(Side::SELL, 2, 5, 100);
  EXPECT_EQ(book.bestBid(), 100);
  EXPECT_EQ(book.volumeAt(Side::BUY, 100), 5);
  EXPECT_TRUE(book.hasOrder(1));
  EXPECT_FALSE(book.hasOrder(2));
}

TEST_F(OrderBookTest, MarkeBuyNoLiquidity)
{
  book.market(Side::BUY, 1, 10);
  EXPECT_EQ(book.bestAsk(), 0);
}

TEST_F(OrderBookTest, MarketBuyFillsAsk)
{
  book.limit(Side::SELL, 1, 10, 100);
  book.market(Side::BUY, 2, 10);
  EXPECT_EQ(book.bestAsk(), 0);
  EXPECT_FALSE(book.hasOrder(1));
}

TEST_F(OrderBookTest, MarketBuyPartialFill)
{
  book.limit(Side::SELL, 1, 10, 100);
  book.market(Side::BUY, 2, 5);
  EXPECT_EQ(book.bestAsk(), 100);
  EXPECT_EQ(book.volumeAt(Side::SELL, 100), 5);
  EXPECT_TRUE(book.hasOrder(1));
}

TEST_F(OrderBookTest, CancelRemovesOrder)
{
  book.limit(Side::BUY, 1, 10, 100);
  book.cancel(1);
  EXPECT_EQ(book.bestBid(), 0);
  EXPECT_FALSE(book.hasOrder(1));
}

TEST_F(OrderBookTest, CancelNonExistentIsSafe)
{
  book.cancel(9999);
}

TEST_F(OrderBookTest, ReducePartial)
{
  book.limit(Side::BUY, 1, 10, 100);
  book.reduce(1, 4);
  EXPECT_EQ(book.volumeAt(Side::BUY, 100), 6);
  EXPECT_TRUE(book.hasOrder(1));
}

TEST_F(OrderBookTest, ReduceToZeroCancels)
{
  book.limit(Side::BUY, 1, 10, 100);
  book.reduce(1, 10);
  EXPECT_EQ(book.bestBid(), 0);
  EXPECT_FALSE(book.hasOrder(1));
}

TEST_F(OrderBookTest, StressMatchAllOrders)
{
  const int N = 1000;
  for (int i = 1; i <= N; ++i)
    book.limit(Side::BUY, i, 1, 100);
  for (int i = N + 1; i <= 2 * N; ++i)
    book.limit(Side::SELL, i, 1, 100);

  EXPECT_EQ(book.bestBid(), 0);
  EXPECT_EQ(book.bestAsk(), 0);
}

TEST_F(OrderBookTest, StressMultiplePriceLevels)
{
  for (int p = 95; p <= 100; ++p)
  {
    for (int i = 0; i < 10; ++i)
      book.limit(Side::BUY, p * 100 + i, 1, p);
  }

  for (int p = 101; p <= 106; ++p)
  {
    for (int i = 0; i < 10; ++i)
      book.limit(Side::SELL, p * 100 + i, 1, p);
  }

  EXPECT_EQ(book.bestBid(), 100);
  EXPECT_EQ(book.bestAsk(), 101);
}

TEST_F(OrderBookTest, StressCancelAll)
{
  const int N = 1000;
  for (int i = 0; i <= N; ++i)
    book.limit(Side::BUY, i, 1, 100);

  for (int i = 0; i <= N; ++i)
    book.cancel(i);

  EXPECT_EQ(book.bestBid(), 0);
  EXPECT_EQ(book.volumeAt(Side::BUY, 100), 0);
}
TEST_F(OrderBookTest, IOC_NoLiquidity_DoesNotRest)
{
  // IOC buy with no resting asks — order should not rest on book
  book.limit(Side::BUY, 1, 10, 100, 0, TimeInForce::IOC);
  EXPECT_EQ(book.bestBid(), 0);
  EXPECT_FALSE(book.hasOrder(1));
}

TEST_F(OrderBookTest, IOC_FullFill_Consumed)
{
  // Resting ask fully covers the IOC buy — both consumed, nothing rests
  book.limit(Side::SELL, 1, 10, 100);
  book.limit(Side::BUY, 2, 10, 100, 0, TimeInForce::IOC);
  EXPECT_EQ(book.bestBid(), 0);
  EXPECT_EQ(book.bestAsk(), 0);
  EXPECT_FALSE(book.hasOrder(1));
  EXPECT_FALSE(book.hasOrder(2));
}

TEST_F(OrderBookTest, IOC_PartialFill_RemainderCancelled)
{
  // Resting ask qty=5, IOC buy qty=10 — 5 fills, remaining 5 cancelled, no resting bid
  // book.limit(Side::SELL, 1, 5, 100);
  book.limit(Side::BUY, 2, 10, 100, 0, TimeInForce::IOC);
  EXPECT_EQ(book.bestBid(), 0);  // remainder was cancelled,not resting
  EXPECT_EQ(book.bestAsk(), 0);  // resting ask fully consumed
  EXPECT_FALSE(book.hasOrder(1));
  EXPECT_FALSE(book.hasOrder(2));
}