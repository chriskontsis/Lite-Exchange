#include "FixServer.hpp"
using namespace fix;

int main() {
    boost::asio::io_context io_context;
    FixServer server(io_context, 12345);
    std::thread serverThread([&]() {io_context.run(); });
    serverThread.join();
}