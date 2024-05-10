#ifndef FIX_CLIENT_HPP
#define FIX_CLIENT_HPP

#include <boost/asio.hpp>

class FixClient {
    public:
        FixClient(boost::asio::io_context& io_context, const std::string& server_address, short port)
        : socket_(io_context) {
        boost::asio::ip::tcp::resolver resolver(io_context);
        auto endpoint_iterator = resolver.resolve({server_address, std::to_string(port)});
        boost::asio::connect(socket_, endpoint_iterator);
    }

    void sendData(const std::string& data) {
        boost::asio::write(socket_, boost::asio::buffer(data));
    }

    private:
        boost::asio::ip::tcp::socket socket_;
};

#endif