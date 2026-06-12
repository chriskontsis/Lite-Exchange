#pragma once
#include <cassert>
#include <cstdint>

namespace lx::book
{
template <typename T, uint32_t N>
class Pool
{
 public:
  static constexpr uint32_t INVALID_IDX = UINT32_MAX;
  Pool()
  {
    for (uint32_t i = 0; i < N; ++i)
      free_list_[i] = i;
    free_top_ = N;
  }

  uint32_t alloc()
  {
    if (free_top_ == 0)
      return INVALID_IDX;
    return free_list_[--free_top_];
  }

  void free(uint32_t idx)
  {
    assert(idx < N);
    free_list_[free_top_++] = idx;
  }

  T& operator[](uint32_t idx)
  {
    assert(idx < N);
    return slots_[idx];
  }

  const T& operator[](uint32_t idx) const
  {
    assert(idx < N);
    return slots_[idx];
  }

  T* data() { return slots_; }
  const T* data() const { return slots_; }

  uint32_t capacity() const { return N; }
  uint32_t free_count() const { return free_top_; }

 private:
  T        slots_[N];
  uint32_t free_list_[N];
  uint32_t free_top_;
};
}  // namespace lx::book