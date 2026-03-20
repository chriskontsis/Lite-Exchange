#include <benchmark/benchmark.h>
#include "../src/matching-engine/OrderBook.hpp"

// Scenario 1: limit order insertion w/ no matches
static void BM_LimitInsert(benchmark::State &state)
{
    LOB::LimitOrderBook book;
    LOB::UID uid = 1;
    for (auto _ : state)
        book.limit(LOB::Side::BUY, uid++, 1, 100);
}
BENCHMARK(BM_LimitInsert);

// Scenario 2: market order matching against a pre-populated book
static void BM_MarketMatch(benchmark::State &state)
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
}
BENCHMARK(BM_MarketMatch);

// Scenario 3: limit cross — buy and sell that immediately match
static void BM_LimitCross(benchmark::State &state)
{
    LOB::LimitOrderBook book;
    LOB::UID uid = 1;
    for (auto _ : state)
    {
        book.limit(LOB::Side::BUY, uid++, 1, 100);
        book.limit(LOB::Side::SELL, uid++, 1, 100);
    }
}
BENCHMARK(BM_LimitCross);

// Scenario 4: cancel
static void BM_Cancel(benchmark::State &state)
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
}
BENCHMARK(BM_Cancel);

BENCHMARK_MAIN();
