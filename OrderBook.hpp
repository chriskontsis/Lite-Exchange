#ifndef ORDERBOOK_HPP
#define ORDERBOOK_HPP

#include "Order.hpp"
#include <queue>
#include <vector>
#include <unordered_map>

struct SellComparator;
struct BuyComparator;

using sellBook = std::priority_queue<Order, std::vector<Order>, SellComparator>;
using buyBook = std::priority_queue<Order, std::vector<Order>, BuyComparator>;
using sellBooks = std::unordered_map<std::string, sellBook>;
using buyBooks = std::unordered_map<std::string, buyBook>;

class OrderBook {
    public:
        OrderBook() {}
    private:
        sellBooks sellBooks;
        buyBooks buyBooks;
        std::unordered_map<int, Order> orderHistory;
};



struct SellComparator {
    bool operator()(const Order& left, const Order& right) {
        if(left.price < right.price) return true;
        if(left.price == right.price && left.timeStamp < right.timeStamp) return true;
        return false;
    }
};
struct BuyComparator {
    bool operator()(const Order& left, const Order& right) {
        if(left.price > right.price) return true;
        if(left.price == right.price && left.timeStamp < right.timeStamp) return true;
        return false;
    }
};



#endif