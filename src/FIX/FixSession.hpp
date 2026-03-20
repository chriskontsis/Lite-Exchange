#ifndef FIX_SESSION_HPP
#define FIX_SESSION_HPP

#include <boost/asio.hpp>
#include <memory>
#include <iostream>
#include <cstring>
#include "FixMessage.hpp"
#include "../ipc/MPSC_Queue.hpp"
#include "../ipc/OrderEvent.hpp"
#include "../gateway/SessionRegistry.hpp"

namespace fix
{
    class FixSession : public std::enable_shared_from_this<FixSession>
    {
    private:
        boost::asio::ip::tcp::socket socket_;
        MPSC_Queue<ipc::OrderEvent, 4096>& inputq_;
        gateway::SessionRegistry& registry_;
        LOB::SessionId session_id_ {0};
        static constexpr int BUFFER_SIZE = 1024;
        char data_[BUFFER_SIZE];

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
                        registry_.removeSession(session_id_);
                        return;
                    }
                    std::string_view buf(data_, length);
                    while(!buf.empty())
                    {
                        auto end = buf.find('\n');
                        if(end == std::string_view::npos) break;
                        auto req = FixMessage::parse(buf.substr(0, end));
                        if(req.type != MsgType::UNKNOWN)
                        {
                            std::cout << "Routing order type= " << (int)req.type << " symbol= " << req.symbol << '\n';
                            inputq_.tryPush(ipc::OrderEvent(req, session_id_));
                        }
                        buf.remove_prefix(end+1);
                    }
                    doRead();
                });
        }

    public:
        FixSession(boost::asio::ip::tcp::socket socket, 
                  MPSC_Queue<ipc::OrderEvent, 4096>& inputq, 
                  gateway::SessionRegistry& registry)
            : socket_(std::move(socket)), inputq_(inputq), registry_(registry) 
            { }

        void start()
        {
            session_id_ = registry_.registerSession(shared_from_this());
            doRead();
        }

        void sendData(std::string data)
        {
            auto buf = std::make_shared<std::string>(std::move(data));
            auto self = shared_from_this();

            boost::asio::post(socket_.get_executor(),
                [this, self, buf]()
                {
                    boost::asio::async_write(socket_, boost::asio::buffer(*buf),
                    [self, buf](boost::system::error_code ec, std::size_t)
                    {
                        if (ec) std::cout << "Send error\n";
                    });
                }
            );
        }
    };

} // namespace fix

#endif