add_executable(net_test net_test.cc)
target_include_directories(net_test PRIVATE ${URING_INCLUDE_DIRS})
target_link_libraries(net_test PRIVATE GTest::gtest_main ${URING_LIBRARIES})
add_test(NAME net COMMAND net_test)
