#pragma once

#include <memory>
#include <vector>
#include "ServerBase.hpp"
#include "../fix/FixSession.hpp"
#include "../ipc/MPSC_Queue.hpp"
#include "../ipc/OrderEvent.hpp"
#include "../gateway/SessionRegistry.hpp"
namespace fix
{
    class FixServer : public ServerBase<FixServer>
    {
    public:
        FixServer(boost::asio::io_context &io_context, short port, 
                  MPSC_Queue<ipc::OrderEvent, 4096>& inputq, gateway::SessionRegistry& registry)
            : ServerBase(io_context, port), inputQ_(inputq), registry_(registry) 
            { }

        void onNewConnection(boost::asio::ip::tcp::socket socket)
        {
            auto session = std::make_shared<FixSession>(std::move(socket), inputQ_, registry_);
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
            }), 
            sessions_.end());
        }

        MPSC_Queue<ipc::OrderEvent, 4096>& inputQ_;
        gateway::SessionRegistry& registry_;
        std::vector<std::weak_ptr<FixSession>> sessions_;
    };
} // namespace fix