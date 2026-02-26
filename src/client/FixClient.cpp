#include "FixClient.hpp"

namespace fix
{
    FixClient::FixClient(std::string_view host, short port)
        : socket_(io_context_), work_guard_(boost::asio::make_work_guard(io_context_))
    {
        connect(host, port);
        doRead();
        bg_thread_ = std::thread([this]()
                                 { io_context_.run(); });
    }

    FixClient::~FixClient()
    {
        work_guard_.reset();
        io_context_.stop();
        if (bg_thread_.joinable())
            bg_thread_.join();
    }

    void FixClient::connect(std::string_view host, short port)
    {
        boost::asio::ip::tcp::resolver resolver(io_context_);
        auto endpoints = resolver.resolve(host, std::to_string(port));
        boost::asio::connect(socket_, endpoints);
    }

    void FixClient::doRead()
    {
        socket_.async_read_some(boost::asio::buffer(data_, BUFFER_SIZE),
                                [this](boost::system::error_code ec, std::size_t length) {
                                    if(ec)
                                    {
                                        std::cout << "Disconnected from server\n";
                                        return;
                                    }
                                    std::cout << "Server: " << std::string_view(data_, length) << '\n';
                                    doRead();
                                });
    }
} // namespace fix