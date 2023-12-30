#include "LimitOrderBook.hpp"

using namespace LOB;

void LimitOrderBook::clear()
{
    bids.clear();
    asks.clear();
    uidOrderMap.clear();
}

void LimitOrderBook::limit(Side side, UID orderUID, Quantity quantity, Price price)
{
    if (side == Side::BUY)
        return limitBuy(orderUID, quantity, price);
    else
        return limitSell(orderUID, quantity, price);
}

void LimitOrderBook::limitBuy(UID orderUID, Quantity quantity, Price price)
{
    uidOrderMap.emplace(orderUID, Order(orderUID, quantity, price, Side::BUY));
    if (asks.best != nullptr && price >= asks.best->price)
    {
        asks.market(uidOrderMap.at(orderUID), [&](UID orderUID)
                    { uidOrderMap.erase(orderUID); });
        if (uidOrderMap.at(orderUID).quantity == 0)
        {
            uidOrderMap.erase(orderUID);
            return;
        }
    }
    bids.limit(uidOrderMap.at(orderUID));
}

void LimitOrderBook::limitSell(UID orderUID, Quantity quantity, Price price)
{
    uidOrderMap.emplace(orderUID, Order(orderUID, quantity, price, Side::BUY));
    if (bids.best != nullptr && price <= bids.best->price)
    {
        bids.market(uidOrderMap.at(orderUID), [&](UID orderUID)
                    { uidOrderMap.erase(orderUID); });
        if (uidOrderMap.at(orderUID).quantity == 0)
        {
            uidOrderMap.erase(orderUID);
            return;
        }
    }
    asks.limit(uidOrderMap.at(orderUID));
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
    Order order(orderUID, 0, quantity, Side::BUY);
    asks.market(order, [&](UID orderUID)
                { uidOrderMap.erase(orderUID); });
}

void LimitOrderBook::marketSell(UID orderUID, Quantity quantity)
{
    Order order(orderUID, 0, quantity, Side::SELL);
    bids.market(order, [&](UID orderUID)
                { uidOrderMap.erase(orderUID); });
}

void LimitOrderBook::reduce(UID orderUID, Quantity quantity)
{
    auto &order = uidOrderMap.at(orderUID);
    if (quantity > order.quantity)
    {
        throw "Quantity to high for order";
    }
    order.quantity -= quantity;
    order.limit->volume -= quantity;
    if (order.side == Side::BUY)
    {
        bids.volumeOfOrdersInTree -= quantity;
        if (order.quantity == 0)
        {
            bids.cancel(order);
            uidOrderMap.erase(order.uid);
        }
    }
    else
    {
        asks.volumeOfOrdersInTree -= quantity;
        if (order.quantity == 0)
        {
            asks.cancel(order);
            uidOrderMap.erase(order.uid);
        }
    }
}

void LimitOrderBook::cancel(UID orderUID)
{
    auto &order = uidOrderMap.at(orderUID);
    if (order.side == Side::BUY)
        bids.cancel(order);
    else
        asks.cancel(order);
    uidOrderMap.erase(orderUID);
}