#pragma once

#include <fcntl.h>        // fcntl(), F_GETFL, F_SETFL, O_NONBLOCK
#include <netinet/in.h>   // sockaddr_in, INADDR_ANY, htons(), AF_INET
#include <netinet/tcp.h>  // TCP_NODELAY, IPPROTO_TCP
#include <sys/fcntl.h>    // redundant on macOS — fcntl.h already pulled by <fcntl.h>
#include <sys/socket.h>   // socket(), bind(), listen(), accept(), setsockopt()
#include <unistd.h>       // close(), read(), write()

#include <stdexcept>  // std::runtime_error

namespace net
{

// Puts fd into non-blocking mode. After this, read()/accept() return
// immediately with EAGAIN instead of sleeping when no data is available.
inline void setNonBlocking(int fd)
{
  int flags = ::fcntl(fd, F_GETFL, 0);
  if (flags < 0)
    throw std::runtime_error("fcntl F_GETFL failed");
  if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    throw std::runtime_error("fcntl F_SETFL O_NONBLOCK failed");
}

// Disables Nagle's algorithm. Every write() is sent immediately rather than
// buffered for up to 200ms waiting to coalesce with future writes.
inline void setNoDelay(int fd)
{
  int yes = 1;
  ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
}

// Allows rebinding the port immediately after a crash/restart, bypassing
// the OS TIME_WAIT delay (~60s) that would otherwise give "address in use".
inline void setReuseAddr(int fd)
{
  int yes = 1;
  ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
}

// Creates a non-blocking TCP listen socket on the given port bound to all
// interfaces. Returns the fd. Backlog of 128 means the kernel queues up to
// 128 completed handshakes before refusing new connections.
inline int makeListenSocket(uint16_t port)
{
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    throw std::runtime_error("socket() failed");
  setReuseAddr(fd);

  int rp = 1;
  ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &rp, sizeof(rp));

  setNonBlocking(fd);

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);  // htons: converts port to network byte order (big-endian)

  if (::bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0)
    throw std::runtime_error("bind() failed");
  if (::listen(fd, 128) < 0)
    throw std::runtime_error("listen() failed");

  return fd;
}

// Pops one pending connection off the listen queue and returns a new fd for
// that client. Returns -1 with errno=EAGAIN if no connection is waiting
// (non-blocking). The listen fd stays open — call this in a loop until EAGAIN.
inline int acceptConn(int listen_fd)
{
  sockaddr_in client_addr{};
  socklen_t   len = sizeof(client_addr);
  return ::accept(listen_fd, (sockaddr*)&client_addr, &len);
}

// Closes the fd and releases it back to the kernel. For a TCP socket this
// triggers the FIN handshake if the connection is still open.
inline void closefd(int fd)
{
  ::close(fd);
}

}  // namespace net