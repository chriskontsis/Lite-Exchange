#pragma once
#include <pthread.h>

#include <cstdint>
#include <stdexcept>

namespace lx::util
{
inline void pin_thread(pthread_t thread, uint32_t core)
{
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core, &cpuset);
  if (pthread_setaffinity_np(thread, sizeof(cpuset), &cpuset) != 0)
    throw std::runtime_error("pthread_setaffinity_np failed");
}

inline void pin_current_thread(uint32_t core)
{
  pin_thread(pthread_self(), core);
}
}  // namespace lx::util