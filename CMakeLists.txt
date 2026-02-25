cmake_minimum_required(VERSION 3.20)
project (LiteExchange CXX)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost CONFIG REQUIRED)
find_package(GTest REQUIRED)

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
