#ifndef ASYNC_CON_HANDLER
#define ASYNC_CON_HANDLER

#include <iostream>
#include <memory>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/enable_shared_from_this.hpp>

class ConnectionHandler : public std::enable_shared_from_this<ConnectionHandler>
{
public:
    typedef std::shared_ptr<ConnectionHandler> pointer;

    static pointer createShared(boost::asio::io_context &io_context)
    {
        return pointer(new ConnectionHandler(io_context));
    }

    boost::asio::ip::tcp::socket &socket()
    {
        return socket_;
    }

    void start()
    {
        socket_.async_read_some(
        boost::asio::buffer(data, mx_length),
        boost::bind(&ConnectionHandler::handleRead,
                    shared_from_this(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
    
        socket_.async_write_some(
        boost::asio::buffer(message, mx_length),
        boost::bind(&ConnectionHandler::handleRead,
                    shared_from_this(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
    }

    void handleRead(const boost::system::error_code &err, size_t bytes_transferred)
    {
        if (!err)
        {
            std::cout << data << '\n';
        }
        else
        {
            std::cerr << "error: " << err.message() << '\n';
            socket_.close();
        }
    }

    void handleWrite(const boost::system::error_code &err, size_t bytes_transferred)
    {
        if (!err)
        {
            std::cout << "Server send Hello Message" << '\n';
        }
        else
        {
            std::cerr << "error: " << err.message() << '\n';
            socket_.close();
        }
    }

private:
    boost::asio::ip::tcp::socket socket_;
    boost::asio::io_context io_context_;
    static const u_int32_t mx_length = 1024;
    char data[mx_length];
    std::string message = "Hello from Server";

    ConnectionHandler(boost::asio::io_context &io_context) : socket_(io_context) {}
};
#endif
