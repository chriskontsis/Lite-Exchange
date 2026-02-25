#pragma once

#include <memory>
#include <vector>
#include "ServerBase.hpp"
#include "../fix/FixSession.hpp"
namespace fix
{
    class FixServer : public ServerBase<FixServer>
    {
    public:
        FixServer(boost::asio::io_context &io_context, short port, EngineDispatcher &dispatcher)
            : ServerBase(io_context, port), dispatcher_(dispatcher) {}

        void onNewConnection(boost::asio::ip::tcp::socket socket)
        {
            auto session = std::make_shared<FixSession>(std::move(socket), dispatcher_);
            sessions_.emplace_back(session);
            pruneSessions();
            session->start();
        }

    private:
        void pruneSessions()
        {
            sessions_.erase(std::remove_if(sessions_.begin(), sessions_.end(), 
            [](const std::weak_ptr<FixSession> &s) 
            {
                return s.expired();
            }), sessions_.end());
        }

        EngineDispatcher &dispatcher_;
        std::vector<std::weak_ptr<FixSession>> sessions_;
    };
} // namespace fix