#ifndef FIX_SERVER_INTERFACE
#define FIX_SERVER_INTERFACE

#include <boost/asio.hpp>
#include <cstdint>
#include <iostream>
#include <thread>

class FixServerInterface {
    public:
        FixServerInterface(uint16_t port) 
        : acceptor(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
        {}

        virtual ~FixServerInterface() {
            Stop();
        }

        bool Start() {
            try {
                waitForClient();
                threadContext = std::thread([this]() {io_context.run();});

            }
            catch(std::exception& e) {
                std::cerr << "[SERVER] Exception: " << e.what() << '\n';
                return false;
            }

            std::cout << "[SERVER] Starterd\n";
            return true;
        }

        void Stop() {
            io_context.stop();
            if(threadContext.joinable()) 
                threadContext.join();
        }

        void waitForClient() {
            acceptor.async_accept([this](std::error_code ec, boost::asio::ip::tcp::socket socket) {
                if(!ec) {
                    std::cout << "[SERVER] New Connection: " << socket.remote_endpoint() << '\n';
                }
                else {
                    std::cout << "[SERVER] New Connection Error: " << ec.message() << '\n';
                }
            });
            waitForClient();
        }
    
    private:
        boost::asio::ip::tcp::acceptor acceptor;
        boost::asio::io_context io_context;
        std::thread threadContext;

};


#endif