#pragma once

#include <boost/asio.hpp>
#include <string>
#include <string_view>
#include <iostream>
#include <thread>

namespace fix
{
    class FixClient
    {
    public:
        FixClient(std::string_view host, short port);
        ~FixClient();
        void send(const std::string& msg);
    private:
        void connect(std::string_view host, short port);
        void doRead();

        boost::asio::io_context io_context_;
        boost::asio::ip::tcp::socket socket_;
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
        std::thread bg_thread_;
        static constexpr int BUFFER_SIZE = 1024;
        char data_[BUFFER_SIZE];
    };
} // namespace fix