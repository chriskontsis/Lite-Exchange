#ifndef MATCHING_ENGINE_HPP
#define MATCHING_ENGINE_HPP

#include "Order.hpp"
#include "OrderBook.hpp"
#include "SocketWrapper.hpp"

#include <chrono>
#include <cstdint>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <unordered_map>

class MatchingEngine
{
public:
    MatchingEngine();
    void start();

private:
    void orderMatch(Order &order, SocketWrapper &socketWrapper);
    void orderDelete(int orderId);
    void orderUpdate(int orderId);
    void parseOrders(std::string &orderInfo, const std::string &delimeter, Order &order);

    OrderBook orderBook;
    std::string filename;
    std::string delimeter;
    int currentStamp;
    std::unordered_map<std::string, std::map<double, u_int64_t, std::greater<>>> buyPrices;
    std::unordered_map<std::string, std::map<double, u_int64_t>> sellPrices;
};

#endif