#include <iostream>
#include <string>
#include <boost/asio.hpp>


int main()
{
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::socket socket(io_context);
    
    socket.connect(boost::asio::ip::tcp::endpoint( boost::asio::ip::address::from_string("127.0.0.1"), 1234 ));
    const std::string message {"Hello from client!\n"};
    boost::system::error_code err;
    boost::asio::write(socket, boost::asio::buffer(message), err);
    if(!err)
    {
        std::cout << "Client sent hello message" << '\n';
    }
    else
    {
        std::cout << "Send failed: " << err.message() << '\n';
    }


    boost::asio::streambuf receive_buffer;
    boost::asio::read(socket, receive_buffer, boost::asio::transfer_all(), err);
    if(err && err != boost::asio::error::eof)
    {
        std::cout << "receive failed" << err.message() << '\n';
    }
    else 
    {
        const char* data = boost::asio::buffer_cast<const char*>(receive_buffer.data());
        std::cout << data << '\n';
    }

}