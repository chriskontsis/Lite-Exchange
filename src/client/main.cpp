#include "FixClient.hpp"
#include <thread>
#include <vector>
#include <boost/asio.hpp>

int main() {
    boost::asio::io_context io_context;
    std::vector<std::thread> clientThreads;
    for (int i = 0; i < 5; ++i) {
        clientThreads.emplace_back([&io_context]() {
            FixClient client(io_context, "127.0.0.1", 12345);
            client.sendData("Hello from client");
        });
    }

        // Join client threads
    for (auto& thread : clientThreads) {
        thread.join();
    }

}