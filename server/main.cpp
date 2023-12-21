#include "MatchingEngine.hpp"
#include <iostream>

int main(int argc, char **argv)
{
    MatchingEngine engine;
    engine.start();
}

// #include <iostream>
// #include <boost/asio.hpp>
// #include "SocketWrapper.hpp"

// int main() {
//     try {
//         boost::asio::io_context io_context;
//         SocketWrapper socketWrapper(io_context, "127.0.0.1", 8080);

//         // Data to be sent to the server (Replace this with your order book update)
//         std::string data_to_send = "Your order book update data";

//         // Send data to the server
//         socketWrapper.writeToSocket(data_to_send);

//         std::cout << "Data sent to server: " << data_to_send << std::endl;

//         // No need to explicitly close the socket (handled in SocketWrapper)

//     } catch (std::exception& e) {
//         std::cerr << "Exception: " << e.what() << std::endl;
//     }
//     return 0;
// }
