#include "SocketWrapper.hpp"



SocketWrapper::SocketWrapper(boost::asio::io_context &io_context, const std::string& host, int port)
        : socket_(io_context), endpoint_(boost::asio::ip::address::from_string(host), port)
    {
        connect();
    }
void SocketWrapper::writeToSocket(const std::string_view &data)
{
        boost::system::error_code error;
        boost::asio::write(socket_, boost::asio::buffer(data), error);
        if (error) {
            std::cerr << "Error writing to socket: " << error.message() << std::endl;
        }
}

void SocketWrapper::connect() {
        boost::system::error_code error;
        socket_.connect(endpoint_, error);
        if (error) {
            std::cerr << "Error connecting to server: " << error.message() << std::endl;
        }
    }