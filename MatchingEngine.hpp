#ifndef MATCHING_ENGINE_HPP
#define MATCHING_ENGINE_HPP

#include "Order.hpp"
#include "OrderBook.hpp"
#include <string> 
#include <fstream>
#include <iostream>
#include <boost/asio.hpp>

class MatchingEngine {
    public:
        MatchingEngine(int argc, char** argv);
        void start();

    private:
        void orderMatch(Order& order);
        void parseOrders(std::string& orderInfo, const std::string& delimeter, Order& order, boost::asio::ip::tcp::socket& socket);
        OrderBook orderBook;

        std::string filename;
        std::string delimeter;
        
};


#endif