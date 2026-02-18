#include "OrderBook.hpp"
using namespace LOB;

void LimitOrderBook::limit(Side side, UID orderUID, Quantity quantity, Price price)
{
    if (side == Side::BUY)
        return limitBuy(orderUID, quantity, price);
    else
        return limitSell(orderUID, quantity, price);
}

void LimitOrderBook::limitBuy(UID orderUID, Quantity quantity, Price price)
{
    UIDtoOrderMap.emplace(orderUID, std::make_shared<Order>(orderUID, price, Side::BUY, quantity));
    if (asks.best != nullptr && price >= asks.best->priceAtLimit)
    {
        asks.market(UIDtoOrderMap.at(orderUID), [&](UID orderUID)
                    { UIDtoOrderMap.erase(orderUID); });
        if (UIDtoOrderMap.at(orderUID)->quantity == 0)
        {
            UIDtoOrderMap.erase(orderUID);
            return;
        }
    }
    bids.limit(UIDtoOrderMap.at(orderUID));
}

void LimitOrderBook::limitSell(UID orderUID, Quantity quantity, Price price)
{
    UIDtoOrderMap.emplace(orderUID, std::make_shared<Order>(orderUID, price, Side::SELL, quantity));
    if (bids.best != nullptr && price <= bids.best->priceAtLimit)
    {
        bids.market(UIDtoOrderMap.at(orderUID), [&](UID orderUID)
                    { UIDtoOrderMap.erase(orderUID); });
        if (UIDtoOrderMap.at(orderUID)->quantity == 0)
        {
            UIDtoOrderMap.erase(orderUID);
            return;
        }
    }
    asks.limit(UIDtoOrderMap.at(orderUID));
}

void LimitOrderBook::market(Side side, UID orderUID, Quantity quantity)
{   
    if (side == Side::BUY)
        return marketBuy(orderUID, quantity);
    else
        return marketSell(orderUID, quantity);
}

void LimitOrderBook::marketBuy(UID orderUID, Quantity quantity)
{
    auto order = std::make_shared<Order>(orderUID, 0, Side::BUY, quantity);
    asks.market(order, [&](UID orderUID)
                { UIDtoOrderMap.erase(orderUID); });
}

void LimitOrderBook::marketSell(UID orderUID, Quantity quantity)
{
    auto order = std::make_shared<Order>(orderUID, 0, Side::SELL, quantity);
    bids.market(order, [&](UID orderUID)
                { UIDtoOrderMap.erase(orderUID); });
}




