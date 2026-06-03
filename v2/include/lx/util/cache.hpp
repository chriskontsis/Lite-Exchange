#pragma once
#include <cstddef>

namespace lx {
inline constexpr std::size_t kCacheLine = 64;

// Wrapping T so it occupies own cache line for false sharing
// Use any for fields written by one thread and read by another

template <typename T> struct alignas(kCacheLine) CachePadded {
  T value{};
};

static_assert(sizeof(CachePadded<char>) == kCacheLine);
}; // namespace lx