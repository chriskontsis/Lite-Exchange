add_executable(engine_test engine_test.cc)
target_link_libraries(engine_test PRIVATE GTest::gtest_main)
add_test(NAME engine COMMAND engine_test)

add_executable(shard_router_test shard_router_test.cc)
target_link_libraries(shard_router_test PRIVATE GTest::gtest_main)
add_test(NAME shard_router COMMAND shard_router_test)
