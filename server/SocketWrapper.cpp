#include "SocketWrapper.hpp"

boost::asio::ip::tcp::socket &SocketWrapper::getSocket()
{
    return socket_;
}

void SocketWrapper::writeToSocket(const std::string &data)
{
    try
    {
        boost::asio::write(socket_, boost::asio::buffer(data));
    }
    catch (const boost::system::system_error &e)
    {
        std::cerr << "Error occurred while writing to socket: " << e.what() << std::endl;
    }
}