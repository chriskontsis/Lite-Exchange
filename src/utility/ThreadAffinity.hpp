#pragma once

#ifdef __linux__
  #include <pthread.h>
  #include <sched.h>

namespace util
{
inline void pinToCore(int core)
{
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}
}  // namespace util

#else
namespace util
{
inline void pinToCore(int) {}  // no-op on macOS
}  // namespace util

#endif