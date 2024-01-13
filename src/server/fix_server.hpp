#ifndef FIX_SERVER
#define FIX_SERVER

#include <iostream>
#include <boost/asio.hpp>

#include "async_connect_handler.hpp"

class FIXServer
{
public:
    FIXServer(boost::asio::io_context& io_context) : io_context_(io_context),
    acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 1234)) 
    {
        startAccept();
    }
private:
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::io_context& io_context_;
    void startAccept();
    void handleAccept(std::shared_ptr<ConnectionHandler> connection,  const boost::system::error_code& err);
};
#endif