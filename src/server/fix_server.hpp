#ifndef FIX_SERVER
#define FIX_SERVER

#include <iostream>
#include <boost/asio.hpp>

class FIXServer
{
public:
    FIXServer(boost::asio::io_context& io_context) : 
    acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 1234)) 
    {
        startAccept();
    }
private:
    boost::asio::ip::tcp::acceptor acceptor_;
    void startAccept();
    void handleAccept();
};
#endif