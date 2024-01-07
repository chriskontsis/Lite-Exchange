#include "FIXServerApplication.hpp"

void FIXServerApplication::onMessage(const FIX42::NewOrderSingle&, const FIX::SessionID& )
{
    FIX::Symbol symbol;
    FIX::Side side;
    FIX::OrdType orderType;
    FIX::OrderQty orderQty;
    FIX::Price price;
    FIX::ClOrdID clOrdID;
    FIX::Account account;

    std::cout << "MESSAGE RECIEVED" << '\n';
    
}