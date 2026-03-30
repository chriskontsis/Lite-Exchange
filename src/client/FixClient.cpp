#include "FixClient.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#ifdef __x86_64__
  #include <x86intrin.h>
static inline uint64_t read_tsc()
{
  return __rdtsc();
}
#elif defined(__aarch64__)
static inline uint64_t read_tsc()
{
  uint64_t val;
  __asm__ volatile("mrs %0, cntvct_el0" : "=r"(val));
  return val;
}
#endif

namespace fix
{
FixClient::FixClient(std::string_view host, short port, MessageCallback on_message)
    : on_message_(std::move(on_message))
{
  connect(host, port);
  bg_thread_ = std::thread([this]() { doRead(); });
}

FixClient::~FixClient()
{
  running_.store(false, std::memory_order_relaxed);
  ::shutdown(fd_, SHUT_RDWR);
  if (bg_thread_.joinable())
    bg_thread_.join();
  ::close(fd_);
}

void FixClient::enableLatencyTracking()
{
  // calibrate: measure rdtsc ticks per ns over 10 ms window
  auto     t0 = std::chrono::steady_clock::now();
  uint64_t r0 = read_tsc();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  uint64_t r1 = read_tsc();
  auto     t1 = std::chrono::steady_clock::now();
  double   ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
  ticks_per_ns_ = static_cast<double>(r1 - r0) / ns;
  histogram_.resize(10000, 0);
  tracking_ = true;
}

void FixClient::connect(std::string_view host, short port)
{
  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo*   res = nullptr;
  std::string host_str(host);
  std::string port_str = std::to_string(port);

  if (::getaddrinfo(host_str.c_str(), port_str.c_str(), &hints, &res) != 0)
    throw std::runtime_error("getaddrinfo failed");

  fd_ = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (fd_ < 0)
  {
    ::freeaddrinfo(res);
    throw std::runtime_error("socket() failed");
  }

  if (::connect(fd_, res->ai_addr, res->ai_addrlen) < 0)
  {
    freeaddrinfo(res);
    throw std::runtime_error("connect() failed");
  }

  ::freeaddrinfo(res);
}

void FixClient::doRead()
{
  while (running_.load(std::memory_order_relaxed))
  {
    ssize_t n = ::read(fd_, data_, BUFFER_SIZE);
    if (n > 0)
    {
      if (tracking_)
      {
        uint64_t         now = read_tsc();
        std::string_view sv(data_, n);
        auto             pos = sv.find("11=");

        if (pos != std::string_view::npos)
        {
          uint64_t uid = std::stoull(std::string(sv.substr(pos + 3)));
          auto     it = send_times_.find(uid);
          if (it != send_times_.end())
          {
            double   ns = static_cast<double>(now - it->second) / ticks_per_ns_;
            uint64_t us = static_cast<uint64_t>(ns / 1000.0);
            if (us < histogram_.size())
              histogram_[us]++;
            send_times_.erase(it);
          }
        }
      }
      if (on_message_)
        on_message_(std::string_view(data_, n));
      else
        std::cout << "Server: " << std::string_view(data_, n) << '\n';
    }
    else
    {
      return;
    }
  }
}

void FixClient::send(const std::string& msg)
{
  uint64_t ts = tracking_ ? read_tsc() : 0;
  ::write(fd_, msg.data(), msg.size());
  if (tracking_)
  {
    auto pos = msg.find("11=");
    if (pos != std::string::npos)
    {
      uint64_t uid = std::stoull(msg.substr(pos + 3));
      send_times_[uid] = ts;
    }
  }
}

std::vector<uint64_t> FixClient::latencyHistogram() const
{
  return histogram_;
}

}  // namespace fix