#pragma once
#include <cstdint>
#include <ctime>

namespace lx::util
{
inline constexpr uint64_t NS_PER_SEC = 1'000'000'000ULL;

inline uint64_t rdtscp()
{
  uint32_t lo, hi, aux;
  __asm__ volatile("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux) :: "memory");
  return (static_cast<uint64_t>(hi) << 32) | lo;
}

inline uint64_t tsc_hz()
{
  constexpr uint64_t SAMPLE_NS = 100'000'000ULL;
  struct timespec    t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  uint64_t c0 = rdtscp();
  do
  {
    clock_gettime(CLOCK_MONOTONIC, &t1);
  } while ((t1.tv_sec - t0.tv_sec) * NS_PER_SEC + (t1.tv_nsec - t0.tv_nsec) < SAMPLE_NS);
  uint64_t c1 = rdtscp();
  uint64_t ns = (t1.tv_sec - t0.tv_sec) * NS_PER_SEC + (t1.tv_nsec - t0.tv_nsec);
  return (c1 - c0) * NS_PER_SEC / ns;
}

inline uint64_t tsc_to_ns(uint64_t ticks, uint64_t hz)
{
  return ticks * NS_PER_SEC / hz;
}
}  // namespace lx::util
