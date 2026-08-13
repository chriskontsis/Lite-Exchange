#pragma once
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <unordered_map>

#include "lx/net/session.hpp"
#include "lx/proto/messages.hpp"

namespace lx::net
{
// Single-threaded epoll reactor bridging client sockets to a Shard.
template <typename Shard>
class Gateway
{
 public:
  static constexpr int MAX_EVENTS = 64;
  Gateway(Shard& shard, uint16_t port) : shard_(shard)
  {
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0)
      throw std::runtime_error("socket() failed");

    int one = 1;
    ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
      throw std::runtime_error("bind() failed");
    if (::listen(listen_fd_, 128) != 0)
      throw std::runtime_error("listen() failed");

    sockaddr_in bound{};
    socklen_t   len = sizeof(bound);
    ::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&bound), &len);
    port_ = ntohs(bound.sin_port);

    set_nonblocking(listen_fd_);

    epoll_fd_ = ::epoll_create1(0);
    if (epoll_fd_ < 0)
      throw std::runtime_error("epoll_create1() failed");
    add_epoll(listen_fd_);
  }

  ~Gateway()
  {
    for (auto& [fd, sess] : sessions_)
      ::close(fd);
    if (epoll_fd_ >= 0)
      ::close(epoll_fd_);
    if (listen_fd_ >= 0)
      ::close(listen_fd_);
  }

  Gateway(const Gateway&) = delete;
  Gateway& operator=(const Gateway&) = delete;

  uint16_t port() const { return port_; }
  void poll_once(int timeout_ms)
  {
    epoll_event events[MAX_EVENTS];
    int         n = ::epoll_wait(epoll_fd_, events, MAX_EVENTS, timeout_ms);
    for (int i = 0; i < n; ++i)
    {
      if (events[i].data.fd == listen_fd_)
        accept_all();
      else
        read_session(events[i].data.fd);
    }
    drain_outbound();
  }

  void run()
  {
    while (running_.load(std::memory_order_relaxed))
      poll_once(10);
  }

  void stop() { running_.store(false, std::memory_order_relaxed); }

 private:
  void add_epoll(int fd)
  {
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);
  }

  void accept_all()
  {
    while (true)
    {
      int cfd = ::accept(listen_fd_, nullptr, nullptr);
      if (cfd < 0)
        break;  // EAGAIN: no pending
      set_nonblocking(cfd);
      int one = 1;
      ::setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
      add_epoll(cfd);
      uint32_t sid = next_session_id_++;
      sessions_.try_emplace(cfd, cfd, sid);
      sid_to_fd_[sid] = cfd;  // reverse map for routing outbound replies
    }
  }

  void read_session(int fd)
  {
    auto it = sessions_.find(fd);
    if (it == sessions_.end())
      return;
    Session& sees = it->second;

    while (true)
    {
      std::byte buf[4096];
      ssize_t   r = ::recv(fd, buf, sizeof(buf), 0);
      if (r > 0)
      {
        sees.append(buf, static_cast<uint32_t>(r));
        sees.consume(shard_.inbound());
      }
      else if (r == 0)
      {
        close_session(fd);
        return;
      }
      else
      {
        break;
      }
    }

    if (sees.closed())
      close_session(fd);
  }

  void close_session(int fd)
  {
    auto it = sessions_.find(fd);
    if (it != sessions_.end())
      sid_to_fd_.erase(it->second.session_id);
    ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    ::close(fd);
    sessions_.erase(fd);
  }

  void drain_outbound()
  {
    proto::OutboundMsg out{};
    while (shard_.outbound().pop(out))
    {
      // Route to the stamped session; drop if that client has disconnected.
      auto it = sid_to_fd_.find(out.hdr.session_id);
      if (it != sid_to_fd_.end())
        send_all(it->second, &out, out.hdr.len);
    }
  }

  static void send_all(int fd, const void* data, uint16_t len)
  {
    const auto* p = static_cast<const std::byte*>(data);
    uint16_t    sent = 0;
    while (sent < len)
    {
      ssize_t w = ::send(fd, p + sent, len - sent, MSG_NOSIGNAL);
      if (w <= 0)
        return;
      sent += static_cast<uint16_t>(w);
    }
  }

  static void set_nonblocking(int fd)
  {
    int flags = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  }

  Shard&                           shard_;
  int                              listen_fd_ = -1;
  int                              epoll_fd_ = -1;
  uint16_t                         port_ = 0;
  uint32_t                         next_session_id_ = 1;
  std::unordered_map<int, Session> sessions_;
  std::unordered_map<uint32_t, int> sid_to_fd_;
  std::atomic<bool>                running_{true};
};
}  // namespace lx::net