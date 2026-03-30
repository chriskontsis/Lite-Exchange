#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace fix
{
class FixClient
{
 public:
  using MessageCallback = std::function<void(std::string_view)>;

  FixClient(std::string_view host, short port, MessageCallback on_message = nullptr);
  ~FixClient();
  void send(const std::string& msg);
  void enableLatencyTracking();
  // return histogram: idx = latency in micro sec, val = msg cnt
  std::vector<uint64_t> latencyHistogram() const;

 private:
  void connect(std::string_view host, short port);
  void doRead();

  int                                    fd_{-1};
  std::atomic<bool>                      running_{true};
  std::thread                            bg_thread_;
  MessageCallback                        on_message_;
  static constexpr int                   BUFFER_SIZE = 4096;
  char                                   data_[BUFFER_SIZE];
  bool                                   tracking_{false};
  double                                 ticks_per_ns_{1.0};
  std::unordered_map<uint64_t, uint64_t> send_times_;  // uid → rdtsc at send
  std::vector<uint64_t>                  histogram_;   // index=µs, value=count
};
}  // namespace fix