include(FetchContent)
FetchContent_Declare(
    hdr_histogram
    GIT_REPOSITORY https://github.com/HdrHistogram/HdrHistogram_c.git
    GIT_TAG        0.11.8
)
set(HDR_HISTOGRAM_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(HDR_LOG_REQUIRED            OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(hdr_histogram)

add_executable(latency_bench latency.cc)
target_link_libraries(latency_bench PRIVATE hdr_histogram_static)
# Benchmarks always build Release-like — override sanitisers
target_compile_options(latency_bench PRIVATE -O3 -march=native -fno-omit-frame-pointer)

add_executable(latency_breakdown latency_breakdown.cc)
target_link_libraries(latency_breakdown PRIVATE hdr_histogram_static)
target_compile_options(latency_breakdown PRIVATE -O3 -march=native -fno-omit-frame-pointer)

add_executable(gateway_latency gateway_latency.cc)
target_link_libraries(gateway_latency PRIVATE hdr_histogram_static)
target_compile_options(gateway_latency PRIVATE -O3 -march=native -fno-omit-frame-pointer)

add_executable(gateway_latency_uring gateway_latency_uring.cc)
target_include_directories(gateway_latency_uring PRIVATE ${URING_INCLUDE_DIRS})
target_link_libraries(gateway_latency_uring PRIVATE hdr_histogram_static ${URING_LIBRARIES})
target_compile_options(gateway_latency_uring PRIVATE -O3 -march=native -fno-omit-frame-pointer)

add_executable(gateway_throughput gateway_throughput.cc)
target_include_directories(gateway_throughput PRIVATE ${URING_INCLUDE_DIRS})
target_link_libraries(gateway_throughput PRIVATE ${URING_LIBRARIES})
target_compile_options(gateway_throughput PRIVATE -O3 -march=native -fno-omit-frame-pointer)
