add_executable(book_test book_test.cc)
target_link_libraries(book_test PRIVATE GTest::gtest_main)
add_test(NAME book COMMAND book_test)
