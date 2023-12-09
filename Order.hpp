#ifndef ORDER_HPP
#define ORDER_HPP

#include <cstdint>
#include <string>

enum class OrderAction {
    BUY, SELL, UPDATE, DELETE
};

class Order {
    public:
        Order(){};
        Order(u_int64_t time_stamp, std::string client_name, std::string ticker, u_int64_t price_, 
        u_int64_t quantity_, u_int64_t expiration_, OrderAction action) {}
    
        u_int64_t timeStamp;
        std::string clientName;
        std::string tickerSymbol;
        u_int64_t price;
        u_int64_t quantity;
        static u_int64_t ID;
        OrderAction action;
};


#endif