// #ifndef SOCKET_WRAPPER_HPP
// #define SOCKET_WRAPPER_HPP

// #include <boost/asio.hpp>
// #include <string>
// #include <iostream>
// #include <string_view>

// class SocketWrapper
// {
// public:
//     SocketWrapper(boost::asio::io_context &io_context, int port)
//         : socket_(io_context) {}

//     boost::asio::ip::tcp::socket &getSocket();
//     void writeToSocket(const std::string_view &data);

// private:
//     boost::asio::ip::tcp::acceptor acceptor_;
//     boost::asio::ip::tcp::socket socket_;
// };

// #endif
#ifndef SOCKET_WRAPPER_HPP
#define SOCKET_WRAPPER_HPP

#include <boost/asio.hpp>
#include <string>
#include <iostream>
#include <string_view>

class SocketWrapper
{
public:
    SocketWrapper(boost::asio::io_context &io_context, const std::string& host, int port)
        : socket_(io_context), endpoint_(boost::asio::ip::address::from_string(host), port)
    {
        connect();
    }

    boost::asio::ip::tcp::socket &getSocket();
    void writeToSocket(const std::string_view &data);

private:
    boost::asio::ip::tcp::endpoint endpoint_;
    boost::asio::ip::tcp::socket socket_;

    void connect() {
        boost::system::error_code error;
        socket_.connect(endpoint_, error);
        if (error) {
            std::cerr << "Error connecting to server: " << error.message() << std::endl;
        }
    }
};

#endif
