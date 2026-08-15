add_executable(spsc_test spsc_test.cc)
target_link_libraries(spsc_test PRIVATE GTest::gtest_main)
add_test(NAME spsc COMMAND spsc_test)