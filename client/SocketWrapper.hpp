#ifndef SOCKET_WRAPPER_HPP
#define SOCKET_WRAPPER_HPP

#include <boost/asio.hpp>
#include <string>
#include <iostream>
#include <string_view>

class SocketWrapper
{
public:
    SocketWrapper(boost::asio::io_context &io_context, const std::string& host, int port);
    void writeToSocket(const std::string_view &data);

private:
    boost::asio::ip::tcp::endpoint endpoint_;
    boost::asio::ip::tcp::socket socket_;
    void connect();
};

#endif
