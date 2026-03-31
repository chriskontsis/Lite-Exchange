  #pragma once

  #ifdef __APPLE__
    #include <sys/event.h>
    #include <sys/time.h>
  #elif defined(USE_IO_URING)
    #include <liburing.h>
    #include <poll.h>
    #include <unordered_map>
  #else
    #include <sys/epoll.h>
  #endif

  #include <unistd.h>

  #include <cstdint>
  #include <stdexcept>

  namespace net
  {

  enum class Watch : uint8_t
  {
    Read      = 0b01,
    Write     = 0b10,
    ReadWrite = 0b11,
  };

  struct Event
  {
    int   fd;
    void* ctx;
    bool  readable;
    bool  writable;
  };

  struct EventLoop
  {
  #if defined(__APPLE__)
    int fd_;
  #elif defined(USE_IO_URING)
    io_uring ring_;
    struct FdInfo
    {
      void*    ctx;
      unsigned poll_mask;
    };
    std::unordered_map<int, FdInfo> fd_info_;
  #else
    int fd_;
  #endif

    EventLoop()
    {
  #if defined(__APPLE__)
      fd_ = ::kqueue();
      if (fd_ < 0)
        throw std::runtime_error("EventLoop init failed");
  #elif defined(USE_IO_URING)
      if (io_uring_queue_init(64, &ring_, 0) < 0)
        throw std::runtime_error("EventLoop init failed");
  #else
      fd_ = ::epoll_create1(0);
      if (fd_ < 0)
        throw std::runtime_error("EventLoop init failed");
  #endif
    }

    ~EventLoop()
    {
  #ifdef USE_IO_URING
      io_uring_queue_exit(&ring_);
  #else
      ::close(fd_);
  #endif
    }

    void add(int sock_fd, void* ctx, Watch watch)
    {
      bool read  = (static_cast<uint8_t>(watch) & 0b01) != 0;
      bool write = (static_cast<uint8_t>(watch) & 0b10) != 0;
  #if defined(__APPLE__)
      struct kevent changes[2];
      int           n = 0;
      if (read)
        EV_SET(&changes[n++], sock_fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, ctx);
      if (write)
        EV_SET(&changes[n++], sock_fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, ctx);
      ::kevent(fd_, changes, n, nullptr, 0, nullptr);
  #elif defined(USE_IO_URING)
      unsigned mask     = (read ? POLLIN : 0) | (write ? POLLOUT : 0);
      fd_info_[sock_fd] = {ctx, mask};
      auto* sqe         = io_uring_get_sqe(&ring_);
      io_uring_prep_poll_add(sqe, sock_fd, mask);
      io_uring_sqe_set_data(sqe, (void*)(uintptr_t)sock_fd);
      io_uring_submit(&ring_);
  #else
      epoll_event ev{};
      ev.data.ptr = ctx;
      ev.events   = (read ? EPOLLIN : 0) | (write ? EPOLLOUT : 0) | EPOLLET;
      ::epoll_ctl(fd_, EPOLL_CTL_ADD, sock_fd, &ev);
  #endif
    }

    void mod(int sock_fd, void* ctx, Watch watch)
    {
      bool read  = (static_cast<uint8_t>(watch) & 0b01) != 0;
      bool write = (static_cast<uint8_t>(watch) & 0b10) != 0;
  #if defined(__APPLE__)
      struct kevent changes[4];
      int           n = 0;
      if (read)
        EV_SET(&changes[n++], sock_fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, ctx);
      else
        EV_SET(&changes[n++], sock_fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
      if (write)
        EV_SET(&changes[n++], sock_fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, ctx);
      else
        EV_SET(&changes[n++], sock_fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
      ::kevent(fd_, changes, n, nullptr, 0, nullptr);
  #elif defined(USE_IO_URING)
      unsigned mask     = (read ? POLLIN : 0) | (write ? POLLOUT : 0);
      fd_info_[sock_fd] = {ctx, mask};
      auto* sqe         = io_uring_get_sqe(&ring_);
      io_uring_prep_poll_remove(sqe, (uint64_t)(uintptr_t)sock_fd);
      io_uring_submit(&ring_);
      sqe = io_uring_get_sqe(&ring_);
      io_uring_prep_poll_add(sqe, sock_fd, mask);
      io_uring_sqe_set_data(sqe, (void*)(uintptr_t)sock_fd);
      io_uring_submit(&ring_);
  #else
      epoll_event ev{};
      ev.data.ptr = ctx;
      ev.events   = (read ? EPOLLIN : 0) | (write ? EPOLLOUT : 0) | EPOLLET;
      ::epoll_ctl(fd_, EPOLL_CTL_MOD, sock_fd, &ev);
  #endif
    }

    void remove(int sock_fd)
    {
  #if defined(__APPLE__)
      struct kevent changes[2];
      EV_SET(&changes[0], sock_fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
      EV_SET(&changes[1], sock_fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
      ::kevent(fd_, changes, 2, nullptr, 0, nullptr);
  #elif defined(USE_IO_URING)
      fd_info_.erase(sock_fd);
      auto* sqe = io_uring_get_sqe(&ring_);
      io_uring_prep_poll_remove(sqe, (uint64_t)(uintptr_t)sock_fd);
      io_uring_submit(&ring_);
  #else
      ::epoll_ctl(fd_, EPOLL_CTL_DEL, sock_fd, nullptr);
  #endif
    }

    int wait(Event* out, int max_events, int timeout_ms = -1)
    {
  #if defined(__APPLE__)
      struct kevent    kevents[64];
      struct timespec  ts{};
      struct timespec* tsp{nullptr};
      if (timeout_ms >= 0)
      {
        ts.tv_sec  = timeout_ms / 1000;
        ts.tv_nsec = (timeout_ms % 1000) * 1'000'000;
        tsp        = &ts;
      }
      int n = ::kevent(fd_, nullptr, 0, kevents, max_events, tsp);
      for (int i = 0; i < n; ++i)
      {
        out[i].fd       = static_cast<int>(kevents[i].ident);
        out[i].ctx      = kevents[i].udata;
        out[i].readable = (kevents[i].filter == EVFILT_READ);
        out[i].writable = (kevents[i].filter == EVFILT_WRITE);
      }
      return n < 0 ? 0 : n;
  #elif defined(USE_IO_URING)
      struct __kernel_timespec  ts{};
      struct __kernel_timespec* tsp = nullptr;
      if (timeout_ms >= 0)
      {
        ts.tv_sec  = timeout_ms / 1000;
        ts.tv_nsec = (int64_t)(timeout_ms % 1000) * 1'000'000LL;
        tsp        = &ts;
      }

      io_uring_submit(&ring_);

      struct io_uring_cqe* cqe;
      if (io_uring_wait_cqe_timeout(&ring_, &cqe, tsp) < 0)
        return 0;

      int n = 0;
      while (n < max_events)
      {
        if (io_uring_peek_cqe(&ring_, &cqe) != 0)
          break;
        int fd  = (int)(uintptr_t)io_uring_cqe_get_data(cqe);
        int res = cqe->res;
        io_uring_cqe_seen(&ring_, cqe);
        auto it = fd_info_.find(fd);
        if (it == fd_info_.end() || res < 0)
          continue;
        out[n].fd       = fd;
        out[n].ctx      = it->second.ctx;
        out[n].readable = (res & POLLIN) != 0;
        out[n].writable = (res & POLLOUT) != 0;
        n++;
      }
      

      for (int i = 0; i < n; ++i)
      {
        auto it = fd_info_.find(out[i].fd);
        if (it == fd_info_.end())
          continue;
        auto* sqe = io_uring_get_sqe(&ring_);
        if (!sqe)
          break;
        io_uring_prep_poll_add(sqe, out[i].fd, it->second.poll_mask);
        io_uring_sqe_set_data(sqe, (void*)(uintptr_t)out[i].fd);
      }

      return n;
  #else
      epoll_event epevents[64];
      int         n = ::epoll_wait(fd_, epevents, max_events, timeout_ms);
      for (int i = 0; i < n; ++i)
      {
        out[i].fd       = 0;
        out[i].ctx      = epevents[i].data.ptr;
        out[i].readable = (epevents[i].events & EPOLLIN) != 0;
        out[i].writable = (epevents[i].events & EPOLLOUT) != 0;
      }
      return n < 0 ? 0 : n;
  #endif
    }
  };

  }  // namespace net