#include "MatchingEngine.hpp"


MatchingEngine::MatchingEngine(int argc, char** argv) : delimeter(" "), filename("text_data.txt") {

}

void MatchingEngine::start() {
    Order order;
    std::string orderInfo;

    std::ifstream myFile(filename);
    if(myFile.is_open()) {
        getline(myFile, orderInfo);
        while(getline(myFile, orderInfo)) {
            parseOrders(orderInfo, delimeter, order);
            orderMatch(order);
        }
        myFile.close()
    }
}