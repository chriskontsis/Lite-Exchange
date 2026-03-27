#pragma once
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>

#include "../gateway/SessionRegistry.hpp"
#include "../ipc/MPSC_Queue.hpp"
#include "../ipc/OrderEvent.hpp"
#include "../net/EventLoop.hpp"
#include "../net/IoHandler.hpp"
#include "FixMessage.hpp"
#include "matching-engine/OrderStructures.hpp"

namespace fix
{
using net::Watch;

class FixSession : public net::IoHandler
{
 public:
  FixSession(int fd, net::EventLoop& loop, MPSC_Queue<ipc::OrderEvent, 4096>& inputq,
             gateway::SessionRegistry& registry)
      : fd_(fd), loop_(loop), input_q_(inputq), registry_(registry)
  {
  }

  ~FixSession() { ::close(fd_); }

  void start() { session_id_ = registry_.registerSession(this); }

  void doRead()
  {
    while (true)
    {
      ssize_t n = ::read(fd_, read_buf_ + read_len_, sizeof(read_buf_) - read_len_);
      if (n > 0)
      {
        read_len_ += static_cast<size_t>(n);
        processMessages();
      }
      else if (n == 0)
      {
        disconnected_ = true;
        return;
      }
      else
      {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
          return;
        disconnected_ = true;
        return;
      }
    }
  }

  void sendData(std::string_view data)
  {
    ssize_t written = ::write(fd_, data.data(), data.size());

    if (written == static_cast<ssize_t>(data.size()))
      return;

    std::string remainder;
    if (written < 0)
    {
      if (errno != EAGAIN && errno != EWOULDBLOCK)
        return;
      remainder = std::string(data);
    }
    else
    {
      remainder = std::string(data.data() + written, data.size() - written);
    }

    {
      std::lock_guard<std::mutex> lock(pending_mu_);
      pending_write_ += remainder;
    }

    write_pending_.store(true, std::memory_order_release);
    loop_.mod(fd_, this, Watch::ReadWrite);
  }

  void flushPending()
  {
    std::lock_guard<std::mutex> lock(pending_mu_);
    if (pending_write_.empty())
    {
      write_pending_.store(false, std::memory_order_release);
      return;
    }
    ssize_t n = ::write(fd_, pending_write_.data(), pending_write_.size());
    if (n > 0)
      pending_write_.erase(0, static_cast<size_t>(n));
    if (pending_write_.empty())
      write_pending_.store(false, std::memory_order_release);
  }

  void onReadable() override { doRead(); }

  void onWritable() override
  {
    flushPending();
    if (!hasPendingWrite())
      loop_.mod(fd_, this, Watch::Read);
  }

  int fd() const { return fd_; }
  LOB::SessionId sessionId() const { return session_id_; }
  bool isDisconnected() const { return disconnected_; }
  bool hasPendingWrite() const { return write_pending_.load(std::memory_order_acquire); }
  bool wantsClose() const override { return disconnected_; }

 private:
  void processMessages()
  {
    size_t start = 0;
    for (size_t i = 0; i < read_len_; ++i)
    {
      if (read_buf_[i] == '\n')
      {
        std::string_view msg(read_buf_ + start, i - start);
        dispatchMessage(msg);
        start = i + 1;
      }
    }
    if (start > 0)
    {
      read_len_ -= start;
      std::memmove(read_buf_, read_buf_ + start, read_len_);
    }
  }

  void dispatchMessage(std::string_view msg)
  {
    auto req = FixMessage::parse(msg);
    if (req.type != MsgType::UNKNOWN)
      input_q_.tryPush(ipc::OrderEvent(req, session_id_));
  }

  int                                fd_;
  net::EventLoop&                    loop_;
  MPSC_Queue<ipc::OrderEvent, 4096>& input_q_;
  gateway::SessionRegistry&          registry_;
  LOB::SessionId                     session_id_{0};

  static constexpr size_t BUFFER_SIZE = 4096;
  char                    read_buf_[BUFFER_SIZE]{};
  size_t                  read_len_{0};
  bool                    disconnected_{false};

  std::string       pending_write_;
  std::mutex        pending_mu_;
  std::atomic<bool> write_pending_{false};
};

}  // namespace fix
