add_executable(engine_test engine_test.cc)
target_link_libraries(engine_test PRIVATE GTest::gtest_main)
add_test(NAME engine COMMAND engine_test)

add_executable(shard_router_test shard_router_test.cc)
target_link_libraries(shard_router_test PRIVATE GTest::gtest_main)
add_test(NAME shard_router COMMAND shard_router_test)

add_executable(shard_set_test shard_set_test.cc)
target_link_libraries(shard_set_test PRIVATE GTest::gtest_main)
add_test(NAME shard_set COMMAND shard_set_test)

add_executable(shard_symbols_test shard_symbols_test.cc)
target_link_libraries(shard_symbols_test PRIVATE GTest::gtest_main)
add_test(NAME shard_symbols COMMAND shard_symbols_test)
