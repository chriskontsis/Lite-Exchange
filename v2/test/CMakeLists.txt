add_executable(smoke_test smoke_test.cc)
target_link_libraries(smoke_test PRIVATE GTest::gtest_main)
add_test(NAME smoke COMMAND smoke_test)

add_subdirectory(util)
add_subdirectory(queue)  
# add_subdirectory(proto)    # Phase 2
# add_subdirectory(book)     # Phase 3-4
# add_subdirectory(net)      # Phase 5
# add_subdirectory(engine)   # Phase 6