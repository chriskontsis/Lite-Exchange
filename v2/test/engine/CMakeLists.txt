add_executable(engine_test engine_test.cc)
target_link_libraries(engine_test PRIVATE GTest::gtest_main)
add_test(NAME engine COMMAND engine_test)
