#include "MatchingEngine.hpp"


MatchingEngine::MatchingEngine(int argc, char** argv) : filename("test_data.txt"), delimeter(" ") {

}

void MatchingEngine::start() {
    Order order;
    std::string orderInfo;
    std::ifstream myFile(filename);
    if(myFile.is_open()) {
        while(getline(myFile, orderInfo)) {
            std::cout << orderInfo << '\n';
            // parseOrders(orderInfo, delimeter, order);
            // orderMatch(order);
        }
        myFile.close();
    }
}