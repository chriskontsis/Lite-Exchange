#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_ENABLE_BENCHMARKING
#include <random>
#include <vector>
#include <unordered_set>
#include <string>
#include <sstream>
#include <iostream>
#include "catch.hpp"
#include "../src/LimitOrderBook.cpp"

using namespace LOB;
using Catch::Benchmark::Chronometer;


// ---------------------------------------------------------------------------
// New Limits Test
// ---------------------------------------------------------------------------


inline void spam_limits(LimitOrderBook& book, int count) {
    for (int i = 0; i < count; i++) book.limit(Side::BUY, i, 50, i);
}

TEST_CASE("Spam new Limits") {
    BENCHMARK_ADVANCED("send 1 new limits")(Chronometer cron) {
        auto book = LimitOrderBook();
        cron.measure([&]() {
            spam_limits(book, 1);
        });
    };
    BENCHMARK_ADVANCED("send 10 new limits")(Chronometer cron) {
        auto book = LimitOrderBook();
        cron.measure([&]() {
            spam_limits(book, 10);
        });
    };
    BENCHMARK_ADVANCED("send 100 new limits")(Chronometer cron) {
        auto book = LimitOrderBook();
        cron.measure([&]() {
            spam_limits(book, 100);
        });
    };
    BENCHMARK_ADVANCED("send 1000 new limits")(Chronometer cron) {
        auto book = LimitOrderBook();
        cron.measure([&]() {
            spam_limits(book, 1000);
        });
    };
}

// ---------------------------------------------------------------------------
// Test random submission and cancelation
// ---------------------------------------------------------------------------


inline void RandomCancelsAndOrders(LimitOrderBook& lob, int cnt, int mean=500, int stddev = 30, int cancelEvery = 4)
{
    auto generator = std::default_random_engine();
    auto dist = std::normal_distribution<double>(mean, stddev);
    lob.limit(Side::BUY, 0, 50, dist(generator));
    for(int i = 1; i < cnt; ++i)
    {
        lob.limit(Side::BUY, i, 50, dist(generator));
        if(i % cancelEvery == 0)
            lob.cancel(i-cancelEvery);
    }   
}

TEST_CASE("Randomly cancel orders out of a stream order creation")
{
    BENCHMARK_ADVANCED("send / cancel 10 orders")(Chronometer cron) {
        auto lob = LimitOrderBook();
        cron.measure([&]() {
            RandomCancelsAndOrders(lob,10);
        });
    };
    BENCHMARK_ADVANCED("send / cancel 100 orders")(Chronometer cron) {
        auto lob = LimitOrderBook();
        cron.measure([&]() {
            RandomCancelsAndOrders(lob,100);
        });
    };
    BENCHMARK_ADVANCED("send / cancel 1000 orders")(Chronometer cron) {
        auto lob = LimitOrderBook();
        cron.measure([&]() {
            RandomCancelsAndOrders(lob,1000);
        });
    };
    BENCHMARK_ADVANCED("send / cancel 10000 orders")(Chronometer cron) {
        auto lob = LimitOrderBook();
        cron.measure([&]() {
            RandomCancelsAndOrders(lob,10000);
        });
    };
}