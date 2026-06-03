add_executable(cache_test cache_test.cc)
target_link_libraries(cache_test PRIVATE GTest::gtest_main)
add_test(NAME cache COMMAND cache_test)
