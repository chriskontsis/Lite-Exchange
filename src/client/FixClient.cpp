#include "FixClient.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace fix
{
FixClient::FixClient(std::string_view host, short port,
                     MessageCallback on_message)
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
    if (n > 0) {
      if (on_message_)
        on_message_(std::string_view(data_, n));
      else
        std::cout << "Server: " << std::string_view(data_, n) << '\n';
    } else {
      return;
    }
  }
}

void FixClient::send(const std::string& msg)
{
  ::write(fd_, msg.data(), msg.size());
}
}  // namespace fix