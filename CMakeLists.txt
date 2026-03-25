cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_COMPILER /usr/bin/clang++)
project (LiteExchange CXX)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost CONFIG REQUIRED)
find_package(GTest REQUIRED)
find_package(benchmark REQUIRED)

add_library(exchange_lib
    src/matching-engine/OrderBook.cpp
)

target_include_directories(exchange_lib PUBLIC src)
target_link_libraries(exchange_lib PUBLIC Boost::headers)

add_executable(exchange
    src/server/serverMain.cpp
)

target_link_libraries(exchange PRIVATE exchange_lib)

enable_testing()

add_executable(fix_tests
    test/FIX/Test_FixMessage.cpp
)

target_link_libraries(fix_tests PRIVATE exchange_lib GTest::gtest 
GTest::gtest_main)
add_test(NAME FixMessageTests COMMAND fix_tests)

add_executable(client
    src/client/ClientMain.cpp
    src/client/FixClient.cpp)

target_link_libraries(client PRIVATE exchange_lib)

add_executable(orderbook_tests
            test/OrderBook/Test_OrderBook.cc
)

target_link_libraries(orderbook_tests PRIVATE exchange_lib
GTest::gtest GTest::gtest_main)
add_test(NAME OrderBookTests COMMAND orderbook_tests)

add_executable(spsc_tests
    test/SPSC/Test_SPSC.cc
)       

target_link_libraries(spsc_tests PRIVATE exchange_lib
GTest::gtest GTest::gtest_main)
add_test(NAME SPSCTests COMMAND spsc_tests)


add_executable(integration_tests
    test/Integration/Test_MultiClient.cc
    src/client/FixClient.cpp
)
target_link_libraries(integration_tests PRIVATE exchange_lib 
GTest::gtest GTest::gtest_main)
add_test(NAME IntegrationTests COMMAND integration_tests)

# Benchmarks
add_executable(bench_orderbook bench/BenchOrderBook.cc)
target_link_libraries(bench_orderbook PRIVATE exchange_lib 
benchmark::benchmark)

add_executable(bench_queues bench/BenchQueues.cc)
target_link_libraries(bench_queues PRIVATE exchange_lib
benchmark::benchmark)

add_executable(bench_endtoend
    bench/BenchEndToEnd.cc
    src/client/FixClient.cpp
)
target_link_libraries(bench_endtoend PRIVATE exchange_lib
benchmark::benchmark)