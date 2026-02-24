#ifndef FIX_SESSION_HPP
#define FIX_SESSION_HPP

#include <boost/asio.hpp>
#include <memory>
#include <iostream>
#include "FixMessage.hpp"
#include "../engine/EngineDispatcher.hpp"

namespace fix
{
    class FixSession : public std::enable_shared_from_this<FixSession>
    {
    private:
        boost::asio::ip::tcp::socket socket_;
        EngineDispatcher &dispatcher_;
        static constexpr int BUFFER_SIZE = 1024;
        char data_[BUFFER_SIZE];

    public:
        FixSession(boost::asio::ip::tcp::socket socket, EngineDispatcher &dispatcher)
            : socket_(std::move(socket)), dispatcher_(dispatcher) {}

        void doRead()
        {
            auto self = shared_from_this();
            socket_.async_read_some(
                boost::asio::buffer(data_, BUFFER_SIZE),
                [this, self](boost::system::error_code ec, std::size_t length)
                {
                    if (ec)
                    {
                        std::cout << "Client disconnected\n";
                        return;
                    }
                    auto req = FixMessage::parse(std::string_view(data_, length));
                    if (req.type != MsgType::UNKNOWN)
                        dispatcher_.route(req);
                    doRead();
                });
        }

        void start()
        {
            doRead();
        }
    };

} // namespace fix

#endif