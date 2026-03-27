#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <string_view>
#include <thread>

namespace fix
{
class FixClient
{
 public:
  using MessageCallback = std::function<void(std::string_view)>;

  FixClient(std::string_view host, short port,
            MessageCallback on_message = nullptr);
  ~FixClient();
  void send(const std::string& msg);

 private:
  void connect(std::string_view host, short port);
  void doRead();

  int                  fd_{-1};
  std::atomic<bool>    running_{true};
  std::thread          bg_thread_;
  MessageCallback      on_message_;
  static constexpr int BUFFER_SIZE = 4096;
  char                 data_[BUFFER_SIZE];
};
}  // namespace fix