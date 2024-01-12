#include "fix_server.hpp"
#include "async_connect_handler.hpp"

void FIXServer::startAccept()
{
    auto connection = ConnectionHandler::createShared(acceptor_.get_executor().context());
}

void FIXServer::handleAccept()
{
    
}