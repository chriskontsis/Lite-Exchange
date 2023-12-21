#ifndef SOCKET_WRAPPER_HPP
#define SOCKET_WRAPPER_HPP

#include <boost/asio.hpp>
#include <string>
#include <iostream>
#include <string_view>

class SocketWrapper
{
public:
    SocketWrapper(boost::asio::io_context &io_context, int port)
        : acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
          socket_(io_context)
    {
        acceptor_.accept(socket_);
    }

    boost::asio::ip::tcp::socket &getSocket();
    void writeToSocket(const std::string_view &data);

private:
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::ip::tcp::socket socket_;
};

#endif