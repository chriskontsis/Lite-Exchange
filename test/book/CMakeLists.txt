add_executable(book_test book_test.cc)
target_link_libraries(book_test PRIVATE GTest::gtest_main)
add_test(NAME book COMMAND book_test)

add_executable(level_bitmap_test level_bitmap_test.cc)
target_link_libraries(level_bitmap_test PRIVATE GTest::gtest_main)
add_test(NAME level_bitmap COMMAND level_bitmap_test)
