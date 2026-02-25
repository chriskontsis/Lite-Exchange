#pragma once

#include <boost/asio.hpp>
#include <iostream>

namespace fix {
    template<typename Derived> 
    class ServerBase {
        public:
            ServerBase(boost::asio::io_context& io_context, short port)
            : acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
            {
                startAccept();
            }
        private:
            void startAccept() 
            {
                acceptor_.async_accept([this]
                    (boost::system::error_code ec, 
                    boost::asio::ip::tcp::socket socket)
                {
                    if(!ec) 
                    {
                        std::cout << "New Connection\n";
                        static_cast<Derived*>(this)->onNewConnection(std::move(socket));
                    }
                    startAccept();
                });
            }

            boost::asio::ip::tcp::acceptor acceptor_;
    };
} // namespace fix