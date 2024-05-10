#ifndef FIX_SESSION_HPP
#define FIX_SESSION_HPP

#include <iostream>
#include <boost/asio.hpp>
#include <thread>
#include <vector>
#include <string>

namespace fix
{
    class FixSession : 
        std::enable_shared_from_this<FixSession>
    {
        public:
            FixSession(boost::asio::ip::tcp::socket socket) : socket_(std::move(socket)) {}

            void start()
            {
                doRead();
            }

            void doRead()
            {
                auto self(shared_from_this());
                socket_.async_read_some(
                    boost::asio::buffer(data_, max_length),
                    [this, self](boost::system::error_code ec, std::size_t length)
                    {
                        if (!ec)
                        {
                            std::cout << "Received: " << std::string(data_, length) << std::endl;
                            doRead();
                        }
                    });
            }

            void sendData(const std::string& data) 
            {
                auto self(shared_from_this());
                boost::asio::async_write(
                socket_,
                boost::asio::buffer(data),
                [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                    if (ec) {
                        std::cout << "Error with sending data\n";
                        return;
                    }
                });

            }

        private:
            boost::asio::ip::tcp::socket socket_;
            enum { max_length = 1024};
            char data_[max_length];
    };
}

#endif