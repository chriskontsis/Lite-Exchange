#ifndef SERVER_HPP
#define SERVER_HPP
#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include "../fix/FixSession.hpp"

namespace fix {
class ServerBase {
public:
    ServerBase(boost::asio::io_context& io_context, short port)
        : acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
          socket_(io_context) {
        startAccept();
    }

    void broadcastMessage(const std::string& message, std::shared_ptr<FixSession> sender) {
        for (auto& session : sessions_) {
            if (session != sender) {
                session->sendData(message);
            }
        }
    }

private:
    void startAccept() {
        acceptor_.async_accept(
            socket_,
            [this](boost::system::error_code ec) {
                if (!ec) {
                    std::cout << "New Connection Established\n";
                    auto newSession = std::make_shared<FixSession>(
                        std::move(socket_));

                    sessions_.emplace_back(newSession);
                    newSession->start();
                }

                startAccept();
            });
    }

    void onDisconnect(std::shared_ptr<FixSession> session) {
        auto it = std::find(sessions_.begin(), sessions_.end(), session);
        if (it != sessions_.end()) {
            std::cout << "Connection Disconnected\n";
            sessions_.erase(it);
        }
    }

    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::ip::tcp::socket socket_;
    std::vector<std::shared_ptr<FixSession>> sessions_;
};
}
#endif