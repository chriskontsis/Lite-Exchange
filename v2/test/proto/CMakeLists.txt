add_executable(proto_test proto_test.cc)
target_link_libraries(proto_test PRIVATE GTest::gtest_main)
add_test(NAME proto COMMAND proto_test)