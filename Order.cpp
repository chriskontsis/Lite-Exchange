#include "Order.hpp"

Order::Order(u_int64_t time_stamp, std::string client_name, std::string ticker, u_int64_t price_, 
        u_int64_t quantity_, u_int64_t expiration_, OrderAction action_) : timeStamp(time_stamp), 
        clientName(client_name), tickerSymbol(ticker), price(price_), quantity(quantity_), expiration(expiration_),
        action(action_)
        
        {++ID;}
        
u_int64_t Order::ID = 0;