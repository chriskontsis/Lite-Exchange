#include "fix_server.hpp"


void FIXServer::startAccept()
{
    auto connection = ConnectionHandler::createShared(io_context_);
    acceptor_.async_accept(connection->socket(),
        boost::bind(&FIXServer::handleAccept, this, connection,
        boost::asio::placeholders::error));
}

void FIXServer::handleAccept(std::shared_ptr<ConnectionHandler> connection,  const boost::system::error_code& err)
{
    if(!err)
    {
        std::cout << "started connecition" << '\n';
        connection->start();
    }
    startAccept();
}