#include "OrderBook.hpp"


OrderAction OrderBook::getOrderType(const std::string& order) {
    if(order == "BUY") return OrderAction::BUY;
    else if(order == "SELL") return OrderAction::SELL;
    else if(order == "DEL") return OrderAction::DELETE;
    else return OrderAction::UPDATE;
}