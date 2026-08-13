#pragma once
#include <arpa/inet.h>
#include <liburing.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
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
// Completion-based (io_uring) reactor bridging client sockets to a Shard.
// Same public interface and routing model as the epoll Gateway, but the I/O
// layer is inverted: instead of "wait for readiness then recv", we submit ops
// and process completions where the bytes have already been delivered.
//
// Iteration 1: multishot accept, single-shot recv (re-armed), io_uring send.
// Later: RECV_MULTISHOT + registered buffer ring (kill the double copy) and
// SQPOLL (kill the submit syscall).
template <typename Shard>
class UringGateway
{
 public:
  static constexpr unsigned RING_DEPTH  = 256;
  static constexpr uint32_t SEND_SLOTS  = 512;
  static constexpr uint32_t INVALID_SLOT = UINT32_MAX;

  // sqpoll: kernel thread polls the SQ so the hot loop makes no submit syscall.
  // sq_cpu: pin that poll thread to a core (UINT32_MAX = let the kernel choose).
  UringGateway(Shard& shard, uint16_t port, bool sqpoll = false,
               uint32_t sq_cpu = UINT32_MAX)
      : shard_(shard)
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

    io_uring_params params{};
    if (sqpoll)
    {
      params.flags = IORING_SETUP_SQPOLL;
      params.sq_thread_idle = 2000;  // ms the poll thread stays awake while idle
      if (sq_cpu != UINT32_MAX)
      {
        params.flags |= IORING_SETUP_SQ_AFF;
        params.sq_thread_cpu = sq_cpu;
      }
    }
    if (io_uring_queue_init_params(RING_DEPTH, &ring_, &params) != 0)
      throw std::runtime_error("io_uring_queue_init_params() failed");

    for (uint32_t i = 0; i < SEND_SLOTS; ++i)
      free_slots_[i] = i;
    free_top_ = SEND_SLOTS;

    arm_accept();
    io_uring_submit(&ring_);
  }

  ~UringGateway()
  {
    io_uring_queue_exit(&ring_);
    for (auto& [fd, conn] : conns_)
      ::close(fd);
    if (listen_fd_ >= 0)
      ::close(listen_fd_);
  }

  UringGateway(const UringGateway&)            = delete;
  UringGateway& operator=(const UringGateway&) = delete;

  uint16_t port() const { return port_; }

  void poll_once(int timeout_ms)
  {
    drain_outbound();          // outbound queue -> send SQEs
    io_uring_submit(&ring_);   // publish all prepared SQEs

    if (timeout_ms > 0)
    {
      io_uring_cqe*     cqe = nullptr;
      __kernel_timespec ts{.tv_sec = timeout_ms / 1000,
                           .tv_nsec = static_cast<long long>(timeout_ms % 1000) * 1'000'000LL};
      io_uring_wait_cqe_timeout(&ring_, &cqe, &ts);  // block up to timeout for >=1 CQE
    }

    unsigned      head;
    unsigned      count = 0;
    io_uring_cqe* cqe;
    io_uring_for_each_cqe(&ring_, head, cqe)
    {
      handle_cqe(cqe);
      ++count;
    }
    io_uring_cq_advance(&ring_, count);
  }

  void run()
  {
    while (running_.load(std::memory_order_relaxed))
      poll_once(10);
  }

  void stop() { running_.store(false, std::memory_order_relaxed); }

 private:
  // ----- user_data tagging: top byte = op, low bytes = fd or send slot -----
  enum Op : uint8_t
  {
    OP_ACCEPT = 0,
    OP_RECV   = 1,
    OP_SEND   = 2
  };
  static uint64_t tag(Op op, uint64_t payload) { return (static_cast<uint64_t>(op) << 56) | payload; }
  static Op       tag_op(uint64_t d) { return static_cast<Op>(d >> 56); }
  static uint64_t tag_payload(uint64_t d) { return d & 0x00FFFFFFFFFFFFFFULL; }

  struct Conn
  {
    Session   session;
    std::byte buf[4096];
    Conn(int fd, uint32_t sid) : session(fd, sid) {}
  };

  io_uring_sqe* get_sqe()
  {
    io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    if (!sqe)
    {
      io_uring_submit(&ring_);  // ring full: flush and retry
      sqe = io_uring_get_sqe(&ring_);
    }
    return sqe;
  }

  void arm_accept()
  {
    io_uring_sqe* sqe = get_sqe();
    io_uring_prep_multishot_accept(sqe, listen_fd_, nullptr, nullptr, 0);
    io_uring_sqe_set_data64(sqe, tag(OP_ACCEPT, 0));
  }

  void arm_recv(int fd)
  {
    Conn&         conn = conns_.at(fd);
    io_uring_sqe* sqe = get_sqe();
    io_uring_prep_recv(sqe, fd, conn.buf, sizeof(conn.buf), 0);
    io_uring_sqe_set_data64(sqe, tag(OP_RECV, static_cast<uint64_t>(fd)));
  }

  void handle_cqe(io_uring_cqe* cqe)
  {
    uint64_t data = io_uring_cqe_get_data64(cqe);
    switch (tag_op(data))
    {
      case OP_ACCEPT:
        on_accept(cqe);
        break;
      case OP_RECV:
        on_recv(static_cast<int>(tag_payload(data)), cqe->res);
        break;
      case OP_SEND:
        free_send_slot(static_cast<uint32_t>(tag_payload(data)));
        break;
    }
  }

  void on_accept(io_uring_cqe* cqe)
  {
    if (cqe->res >= 0)
    {
      int cfd = cqe->res;
      int one = 1;
      ::setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
      uint32_t sid = next_session_id_++;
      conns_.try_emplace(cfd, cfd, sid);
      sid_to_fd_[sid] = cfd;
      arm_recv(cfd);
    }
    // Multishot terminates if the kernel drops F_MORE; re-arm to keep accepting.
    if (!(cqe->flags & IORING_CQE_F_MORE))
      arm_accept();
  }

  void on_recv(int fd, int res)
  {
    auto it = conns_.find(fd);
    if (it == conns_.end())
      return;
    if (res <= 0)  // 0 = peer closed, <0 = error
    {
      close_conn(fd);
      return;
    }
    Conn& conn = it->second;
    conn.session.append(conn.buf, static_cast<uint32_t>(res));
    conn.session.consume(shard_.inbound());
    if (conn.session.closed())
    {
      close_conn(fd);
      return;
    }
    arm_recv(fd);  // single-shot: re-arm for the next chunk
  }

  void close_conn(int fd)
  {
    auto it = conns_.find(fd);
    if (it != conns_.end())
      sid_to_fd_.erase(it->second.session.session_id);
    ::close(fd);
    conns_.erase(fd);
  }

  void drain_outbound()
  {
    proto::OutboundMsg out{};
    while (shard_.outbound().pop(out))
    {
      auto it = sid_to_fd_.find(out.hdr.session_id);
      if (it == sid_to_fd_.end())
        continue;  // client disconnected
      int      fd = it->second;
      uint32_t slot = alloc_send_slot();
      if (slot == INVALID_SLOT)
      {
        ::send(fd, &out, out.hdr.len, MSG_NOSIGNAL);  // pool full: blocking fallback
        continue;
      }
      send_pool_[slot] = out;  // stable storage until the send completes
      io_uring_sqe* sqe = get_sqe();
      io_uring_prep_send(sqe, fd, &send_pool_[slot], out.hdr.len, MSG_NOSIGNAL);
      io_uring_sqe_set_data64(sqe, tag(OP_SEND, slot));
    }
  }

  uint32_t alloc_send_slot() { return free_top_ ? free_slots_[--free_top_] : INVALID_SLOT; }
  void     free_send_slot(uint32_t slot) { free_slots_[free_top_++] = slot; }

  Shard&                            shard_;
  io_uring                          ring_{};
  int                               listen_fd_ = -1;
  uint16_t                          port_      = 0;
  uint32_t                          next_session_id_ = 1;
  std::unordered_map<int, Conn>     conns_;
  std::unordered_map<uint32_t, int> sid_to_fd_;
  std::atomic<bool>                 running_{true};

  proto::OutboundMsg send_pool_[SEND_SLOTS]{};
  uint32_t           free_slots_[SEND_SLOTS];
  uint32_t           free_top_ = 0;
};
}  // namespace lx::net
