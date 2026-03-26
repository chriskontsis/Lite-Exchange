#pragma once

#ifdef __APPLE__
#include <sys/event.h>
#include <sys/time.h>
#else
#include <sys/epoll.h>
#endif

#include <stdexcept>
#include <unistd.h>

namespace net {
struct Event {
  void *ctx;
  bool readable;
  bool writable;
};

struct EventLoop {
  int fd_;

  EventLoop() {
#ifdef __APPLE__
    fd_ = ::kqueue();
#else
    fd_ = ::epoll_create1(0);
#endif

    if (fd_ < 0)
      throw std::runtime_error("EventLoop init failed");
  }

  ~EventLoop() { ::close(fd_); }

  void add(int sock_fd, void *ctx, bool read, bool write) {
#ifdef __APPLE__
    struct kevent changes[2];
    int n = 0;
    if (read)
      EV_SET(&changes[n++], sock_fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, ctx);
    if (write)
      EV_SET(&changes[n++], sock_fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0,
             ctx);
    ::kevent(fd_, changes, n, nullptr, 0, nullptr);
#else
    epoll_event ev{};
    ev.data.ptr = ctx;
    ev.events = (read ? EPOLLIN : 0) | (write ? EPOLLOUT : 0) | EPOLLET;
    ::epoll_ctl(fd_, EPOLL_CTL_ADD, sock_fd, &ev);
#endif
  }

  void mod(int sock_fd, void *ctx, bool read, bool write) {
#ifdef __APPLE__
    struct kevent changes[4];
    int n = 0;

    if (read)
      EV_SET(&changes[n++], sock_fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, ctx);
    else
      EV_SET(&changes[n++], sock_fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);

    if (write)
      EV_SET(&changes[n++], sock_fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0,
             ctx);
    else
      EV_SET(&changes[n++], sock_fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);

    ::kevent(fd_, changes, n, nullptr, 0, nullptr);
#else
    epoll_event ev{};
    ev.data.ptr = ctx;
    ev.events = (read ? EPOLLIN : 0) | (write ? EPOLLOUT : 0) | EPOLLET;
    ::epoll_ctl(fd_, EPOLL_CTL_MOD, sock_fd, &ev);
#endif
  }

  void remove(int sock_fd) {
#ifdef __APPLE__
    struct kevent changes[2];
    EV_SET(&changes[0], sock_fd, EVFILT_READ,  EV_DELETE, 0, 0, nullptr);
    EV_SET(&changes[1], sock_fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
    ::kevent(fd_, changes, 2, nullptr, 0, nullptr);
#else
    ::epoll_ctl(fd_, EPOLL_CTL_DEL, sock_fd, nullptr);
#endif
  }

  int wait(Event *out, int max_events, int timeout_ms = -1) {
#ifdef __APPLE__
    struct kevent kevents[64];
    struct timespec ts{};
    struct timespec *tsp{nullptr};

    if (timeout_ms >= 0) {
      ts.tv_sec = timeout_ms / 1000;
      ts.tv_nsec = (timeout_ms % 1000) * 1'000'000;
      tsp = &ts;
    }

    int n = ::kevent(fd_, nullptr, 0, kevents, max_events, tsp);
    for (int i = 0; i < n; ++i) {
      out[i].ctx = kevents[i].udata;
      out[i].readable = (kevents[i].filter == EVFILT_READ);
      out[i].writable = (kevents[i].filter == EVFILT_WRITE);
    }

    return n < 0 ? 0 : n;
#else
    epoll_event epevents[64];
    int n = ::epoll_wait(fd_, epevents, max_events, timeout_ms);
    for (int i = 0; i < n; ++i) {
      out[i].ctx = epevents[i].data.ptr;
      out[i].readable = (epevents[i].events & EPOLLIN) != 0;
      out[i].writable = (epevents[i].events & EPOLLOUT) != 0;
    }
    return n < 0 ? 0 : n;
#endif
  }
};

} // namespace net