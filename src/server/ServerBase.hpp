#pragma once

#include "../net/EventLoop.hpp"
#include "../net/IoHandler.hpp"
#include "../net/SocketUtils.hpp"

namespace fix
{
template <typename Derived>
class ServerBase : public net::IoHandler
{
 public:
  ServerBase(net::EventLoop& loop, uint16_t port)
      : listen_fd_(net::makeListenSocket(port)), loop_(loop)
  {
    loop_.add(listen_fd_, this, net::Watch::Read);
  }
  ~ServerBase()
  {
    loop_.remove(listen_fd_);
    net::closefd(listen_fd_);
  }

  void onReadable() override
  {
    while (true)
    {
      int client_fd = net::acceptConn(listen_fd_);
      if (client_fd < 0)
        return;
      net::setNonBlocking(client_fd);
      net::setNoDelay(client_fd);
      static_cast<Derived*>(this)->onNewConnection(client_fd);
    }
  }

 protected:
  int             listen_fd_;
  net::EventLoop& loop_;
};
}  // namespace fix