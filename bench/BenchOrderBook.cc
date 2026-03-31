#include <benchmark/benchmark.h>

#include <deque>

#include "../src/matching-engine/OrderBook.hpp"

// Scenario 1: limit order insertion w/ no matches — prices cycle across 1000 levels
// so each pass through the cycle creates new Limit objects, testing the level-creation path
static void BM_LimitInsert(benchmark::State& state)
{
  LOB::LimitOrderBook book;
  LOB::UID            uid = 1;
  int                 inserted = 0;
  LOB::Price          price = 1;
  for (auto _ : state)
  {
    if (inserted >= 60000)
    {
      state.PauseTiming();
      book.clear();
      uid = 1;
      inserted = 0;
      price = 1;
      state.ResumeTiming();
    }
    book.limit(LOB::Side::BUY, uid++, 1, price);
    if (++price > 1000)
      price = 1;
    ++inserted;
  }
}
BENCHMARK(BM_LimitInsert);

// Scenario 2: market order matching against a pre-populated book
static void BM_MarketMatch(benchmark::State& state)
{
  for (auto _ : state)
  {
    state.PauseTiming();
    LOB::LimitOrderBook book;
    for (int i = 1; i <= 1000; ++i)
      book.limit(LOB::Side::SELL, i, 1, 100);
    LOB::UID uid = 1001;
    state.ResumeTiming();

    for (int i = 0; i < 1000; ++i)
      book.market(LOB::Side::BUY, uid++, 1);
  }
  state.SetItemsProcessed(state.iterations() * 1000);
}
BENCHMARK(BM_MarketMatch);

// Scenario 2b: market order walks multiple price levels — exercises findBest + level erasure on
// each step 10 sell levels at prices 101-110, 50 qty each; one market buy for 500 drains the entire
// book
static void BM_MarketWalk(benchmark::State& state)
{
  for (auto _ : state)
  {
    state.PauseTiming();
    LOB::LimitOrderBook book;
    LOB::UID            uid = 1;
    for (int level = 0; level < 10; ++level)
      for (int j = 0; j < 50; ++j)
        book.limit(LOB::Side::SELL, uid++, 1, 101 + level);
    LOB::UID aggressor = uid++;
    state.ResumeTiming();

    book.market(LOB::Side::BUY, aggressor, 500);
  }
  state.SetItemsProcessed(state.iterations() * 500);
}
BENCHMARK(BM_MarketWalk);

// Scenario 3: limit cross — buy and sell that immediately match
static void BM_LimitCross(benchmark::State& state)
{
  LOB::LimitOrderBook book;
  LOB::UID            uid = 1;
  for (auto _ : state)
  {
    book.limit(LOB::Side::BUY, uid++, 1, 100);
    book.limit(LOB::Side::SELL, uid++, 1, 100);
  }
}
BENCHMARK(BM_LimitCross);

// Scenario 4: cancel
static void BM_Cancel(benchmark::State& state)
{
  for (auto _ : state)
  {
    state.PauseTiming();
    LOB::LimitOrderBook book;
    for (LOB::UID i = 1; i <= 1000; ++i)
      book.limit(LOB::Side::BUY, i, 1, 100);
    LOB::UID uid = 1;
    state.ResumeTiming();

    for (int i = 0; i < 1000; ++i)
      book.cancel(uid++);
  }
  state.SetItemsProcessed(state.iterations() * 1000);
}
BENCHMARK(BM_Cancel);

// Dense cancel - 100 price levels each with 10 orders, cancel sweeps best inward
static void BM_Cancel_Dense(benchmark::State& state)
{
  for (auto _ : state)
  {
    state.PauseTiming();
    LOB::LimitOrderBook book;
    LOB::UID            uid = 1;

    // 10 orders at each of 100 consec price levels
    for (int price = 1; price <= 100; ++price)
      for (int j = 0; j < 10; ++j)
        book.limit(LOB::Side::BUY, uid++, 1, price);

    state.ResumeTiming();

    for (int price = 100; price >= 1; --price)
      for (int j = 0; j < 10; ++j)
        book.cancel((price - 1) * 10 + j + 1);
  }
  state.SetItemsProcessed(state.iterations() * 1000);
}
BENCHMARK(BM_Cancel_Dense);

// Sparse cancel - 5 orders spread across a wide price range, always cancel best
static void BM_Cancel_Sparse(benchmark::State& state)
{
  for (auto _ : state)
  {
    state.PauseTiming();
    LOB::LimitOrderBook book;

    // 5 orders at prices 100, 300, 500, 700, 900 — wide gaps between levels
    book.limit(LOB::Side::BUY, 1, 1, 100);
    book.limit(LOB::Side::BUY, 2, 1, 300);
    book.limit(LOB::Side::BUY, 3, 1, 500);
    book.limit(LOB::Side::BUY, 4, 1, 700);
    book.limit(LOB::Side::BUY, 5, 1, 900);
    state.ResumeTiming();

    book.cancel(5);
    book.cancel(4);
    book.cancel(3);
    book.cancel(2);
    book.cancel(1);
  }
  state.SetItemsProcessed(state.iterations() * 5);
}
BENCHMARK(BM_Cancel_Sparse)->MinTime(1.0);

// Dense insert — sequential prices, simulates a liquid book building up
static void BM_LimitInsert_Dense(benchmark::State& state)
{
  for (auto _ : state)
  {
    state.PauseTiming();
    LOB::LimitOrderBook book;
    LOB::UID            uid = 1;
    int                 inserted = 0;
    state.ResumeTiming();

    for (int price = 1; price <= 100; ++price)
    {
      if (inserted >= 60000)
      {
        state.PauseTiming();
        book.clear();
        uid = 1;
        inserted = 0;
        state.ResumeTiming();
      }
      book.limit(LOB::Side::BUY, uid++, 1, price);
      ++inserted;
    }
  }
}
BENCHMARK(BM_LimitInsert_Dense);

// Sparse insert — prices spread across a wide range, simulates illiquid name
static void BM_LimitInsert_Sparse(benchmark::State& state)
{
  static const LOB::Price prices[] = {50, 200, 450, 700, 950};
  for (auto _ : state)
  {
    state.PauseTiming();
    LOB::LimitOrderBook book;
    LOB::UID            uid = 1;
    int                 inserted = 0;
    state.ResumeTiming();

    for (auto price : prices)
    {
      if (inserted >= 60000)
      {
        state.PauseTiming();
        book.clear();
        uid = 1;
        inserted = 0;
        state.ResumeTiming();
      }
      book.limit(LOB::Side::BUY, uid++, 1, price);
      ++inserted;
    }
  }
}
BENCHMARK(BM_LimitInsert_Sparse);

// Steady-state mixed workload — constant book depth, interleaved inserts and cancels
// Uses a FIFO deque so cancel always targets the oldest known-live UID — guaranteed
// successful (no no-ops), so pool utilization and hash map load factor stay fixed.
// Market-order matching is covered separately by BM_MarketMatch and BM_MarketWalk.
static void BM_SteadyState(benchmark::State& state)
{
  constexpr int kDepth = 500;
  constexpr int kPriceLevels = 50;

  LOB::LimitOrderBook  book;
  LOB::UID             uid = 1;
  std::deque<LOB::UID> live;

  // pre-populate kDepth resting sell orders across kPriceLevels levels
  for (int i = 0; i < kDepth; ++i)
  {
    book.limit(LOB::Side::SELL, uid, 1, 101 + (i % kPriceLevels));
    live.push_back(uid++);
  }

  for (auto _ : state)
  {
    // cancel oldest live order (guaranteed in book — no market orders racing us)
    book.cancel(live.front());
    live.pop_front();

    // insert one new resting order at a cycling price
    LOB::Price p = 101 + (uid % kPriceLevels);
    book.limit(LOB::Side::SELL, uid, 1, p);
    live.push_back(uid++);
  }
  // 2 ops per iteration: 1 cancel + 1 insert
  state.SetItemsProcessed(state.iterations() * 2);
}
BENCHMARK(BM_SteadyState);

BENCHMARK_MAIN();
