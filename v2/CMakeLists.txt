cmake_minimum_required(VERSION 3.28)
project(lx-v2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# --- Compiler flags ---------------------------------------------------------
add_compile_options(-Wall -Wextra -Wpedantic -Werror)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=address,undefined)
endif()

# Release: max optimisation; LTO (thin); -march=native; no PLT indirection
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_compile_options(-O3 -march=native -fno-plt)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)   # clang: -flto=thin
endif()

# RelWithDebInfo: profile-friendly — optimised but with frame pointers + DWARF
if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  add_compile_options(-O2 -march=native -fno-omit-frame-pointer -g)
endif()

# --- Dependencies (uncommented as phases land) ------------------------------
find_package(GTest REQUIRED)
# find_package(benchmark REQUIRED)           # Phase 7
find_package(PkgConfig REQUIRED)
pkg_check_modules(URING REQUIRED liburing)


include_directories(include)

enable_testing()
add_subdirectory(test)
add_subdirectory(bench)                    # Phase 7
