add_executable(cache_test cache_test.cc)
target_link_libraries(cache_test PRIVATE GTest::gtest_main)
add_test(NAME cache COMMAND cache_test)

add_executable(huge_page_test huge_page_test.cc)
target_link_libraries(huge_page_test PRIVATE GTest::gtest_main)
add_test(NAME huge_page COMMAND huge_page_test)
