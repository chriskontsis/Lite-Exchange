#include "lx/util/cache.hpp"
#include <gtest/gtest.h>

using namespace lx;

TEST(CacheLine, ConstantIs64) { static_assert(kCacheLine == 64); }

TEST(CachePadded, SizeIsMultipleOf64) {
  static_assert(sizeof(CachePadded<char>) % kCacheLine == 0);
  static_assert(sizeof(CachePadded<int>) % kCacheLine == 0);
  static_assert(sizeof(CachePadded<double>) % kCacheLine == 0);
}

TEST(CachePadded, AlignmentIs64) {
  static_assert(alignof(CachePadded<int>) == kCacheLine);
}

TEST(CachePadded, TwoFieldsOnDifferentCacheLines) {
  struct Pair {
    CachePadded<int> head;
    CachePadded<int> tail;
  };
  Pair p{};
  auto a = reinterpret_cast<uintptr_t>(&p.head);
  auto b = reinterpret_cast<uintptr_t>(&p.tail);
  EXPECT_NE(a / kCacheLine, b / kCacheLine);
}

TEST(CachePadded, ValueAccess) {
  CachePadded<int> x{};
  x.value = 42;
  EXPECT_EQ(x.value, 42);
}