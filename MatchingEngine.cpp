#include "MatchingEngine.hpp"
#include <boost/algorithm/string.hpp>


MatchingEngine::MatchingEngine(int argc, char** argv) : filename("test_data.txt"), delimeter(" ") {

}

void MatchingEngine::start() {
    std::string orderInfo;
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::acceptor acceptor(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 8080));
    boost::asio::ip::tcp::socket socket(io_context);
    // acceptor.accept(socket);

    std::ifstream myFile(filename);
    if(myFile.is_open()) {
        while(getline(myFile, orderInfo)) {
            Order order;
            parseOrders(orderInfo, delimeter, order, socket);
            // orderMatch(order);
        }
        myFile.close();
    }
}

void MatchingEngine::parseOrders(std::string& orderInfo, const std::string& delimeter, Order& order, boost::asio::ip::tcp::socket& socket){
    std::vector<std::string> tokens;
    boost::split(tokens, orderInfo, boost::is_any_of(delimeter));

    int orderId = stoi(tokens[0]);
    

    // boost::asio::write(socket, boost::asio::buffer(orderInfo));
    

}

void MatchingEngine::orderMatch(Order& order) {

}