#pragma once
#include <atomic>
#include <cstddef>
#include <type_traits>

#include "lx/util/cache.hpp"

namespace lx
{
template <typename T, std::size_t N>
  requires(N > 0 && (N & (N - 1)) == 0)  // power 2
class SpscQueue
{
 public:
  bool push(const T& val)
  {
    const std::size_t head = head_.value.load(std::memory_order_relaxed);
    const std::size_t next = head + 1;

    if (next - tail_.value.load(std::memory_order_acquire) > N)
      return false;

    slots_[head & kMask] = val;
    head_.value.store(next, std::memory_order_release);
    return true;
  }
  bool pop(T& val)
  {
    const std::size_t tail = tail_.value.load(std::memory_order_acquire);
    if (head_.value.load(std::memory_order_acquire) == tail)
      return false;
    val = slots_[tail & kMask];
    tail_.value.store(tail + 1, std::memory_order_release);

    return true;
  }

 private:
  static constexpr std::size_t          kMask = N - 1;
  CachePadded<std::atomic<std::size_t>> head_{};
  CachePadded<std::atomic<std::size_t>> tail_{};
  T                                     slots_[N]{};
};
}  // namespace lx