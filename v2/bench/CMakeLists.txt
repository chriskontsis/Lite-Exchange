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
