#ifndef MATCHING_ENGINE_HPP
#define MATCHING_ENGINE_HPP

#include "Order.hpp"
#include "OrderBook.hpp"
#include "SocketWrapper.hpp"
#include <string> 
#include <unordered_map>
#include <map>
#include <fstream>
#include <iostream>
#include <cstdint>
#include <boost/asio.hpp>

class MatchingEngine {
    public:
        MatchingEngine(int argc, char** argv);
        void start();

    private:
        void orderMatch(Order& order, SocketWrapper& socketWrapper);
        void orderDelete(int orderId);
        void orderUpdate(int orderId);
        void parseOrders(std::string& orderInfo, const std::string& delimeter, Order& order);
        OrderBook orderBook;


        std::string filename;
        std::string delimeter;
        int currentStamp;
        std::unordered_map<std::string, std::map<double, u_int64_t, std::greater<>>> buyPrices;
        std::unordered_map<std::string, std::map<double, u_int64_t>> sellPrices;
};


#endif