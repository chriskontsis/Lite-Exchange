#include "SocketWrapper.hpp"

boost::asio::ip::tcp::socket &SocketWrapper::getSocket()
{
    return socket_;
}

void SocketWrapper::writeToSocket(const std::string_view &data)
{
        boost::system::error_code error;
        boost::asio::write(socket_, boost::asio::buffer(data), error);
        if (error) {
            std::cerr << "Error writing to socket: " << error.message() << std::endl;
        }
}