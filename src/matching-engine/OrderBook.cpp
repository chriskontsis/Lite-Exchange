#include "OrderBook.hpp"
using namespace LOB;

void LimitOrderBook::limit(Side side, UID orderUID, Quantity quantity, Price price, 
                            const std::function<void(fix::FillReport)>& onFill)
{
    if (side == Side::BUY)
        return limitBuy(orderUID, quantity, price, onFill);
    else
        return limitSell(orderUID, quantity, price, onFill);
}

void LimitOrderBook::limitBuy(UID orderUID, Quantity quantity, Price price, 
                            const std::function<void(fix::FillReport)>& onFill)
{
    UIDtoOrderMap.emplace(orderUID, std::make_shared<Order>(orderUID, price, Side::BUY, quantity));
    UIDtoOrderMap.at(orderUID)->onFill = onFill;

    if (asks.best != nullptr && price >= asks.best->priceAtLimit)
    {
        asks.market(UIDtoOrderMap.at(orderUID), [&](UID matchUID, UID aggressorUID, Quantity qty, Price execPrice, FillType fillType)
        { 
            if(fillType == FillType::FULL) 
                UIDtoOrderMap.erase(matchUID);
            if(onFill)
                onFill(fix::FillReport(aggressorUID, matchUID, qty, execPrice, Side::BUY));
        });
        if (UIDtoOrderMap.at(orderUID)->quantity == 0)
        {
            UIDtoOrderMap.erase(orderUID);
            return;
        }
    }
    bids.limit(UIDtoOrderMap.at(orderUID));
}

void LimitOrderBook::limitSell(UID orderUID, Quantity quantity, Price price, 
                                const std::function<void(fix::FillReport)>& onFill)
{
    UIDtoOrderMap.emplace(orderUID, std::make_shared<Order>(orderUID, price, Side::SELL, quantity));
    UIDtoOrderMap.at(orderUID)->onFill = onFill;

    if (bids.best != nullptr && price <= bids.best->priceAtLimit)
    {
        bids.market(UIDtoOrderMap.at(orderUID), [&](UID matchUID, UID aggressorUID, Quantity qty, Price execPrice, FillType fillType)
        { 
            if(fillType == FillType::FULL)
                UIDtoOrderMap.erase(matchUID);
            if(onFill)
                onFill(fix::FillReport(aggressorUID, matchUID, qty, execPrice, Side::SELL));
        });
        if (UIDtoOrderMap.at(orderUID)->quantity == 0)
        {
            UIDtoOrderMap.erase(orderUID);
            return;
        }
    }
    asks.limit(UIDtoOrderMap.at(orderUID));
}

void LimitOrderBook::market(Side side, UID orderUID, Quantity quantity, const std::function<void(fix::FillReport)>& onFill)
{   
    if (side == Side::BUY)
        return marketBuy(orderUID, quantity, onFill);
    else
        return marketSell(orderUID, quantity, onFill);
}

void LimitOrderBook::marketBuy(UID orderUID, Quantity quantity, const std::function<void(fix::FillReport)>& onFill)
{
    auto order = std::make_shared<Order>(orderUID, 0, Side::BUY, quantity);

    asks.market(order, [&](UID matchUID, UID aggressorUID, Quantity qty, Price execPrice, FillType fillType)
        {
            if(fillType == FillType::FULL)
                UIDtoOrderMap.erase(matchUID);
            if(onFill)
                onFill(fix::FillReport(aggressorUID, matchUID, qty, execPrice, Side::BUY));
        });
}

void LimitOrderBook::marketSell(UID orderUID, Quantity quantity, const std::function<void(fix::FillReport)>& onFill)
{
    auto order = std::make_shared<Order>(orderUID, 0, Side::SELL, quantity);
    
    bids.market(order, [&](UID matchUID, UID aggressorUID, Quantity qty, Price execPrice, FillType fillType)
        {
            if(fillType == FillType::FULL)
                UIDtoOrderMap.erase(matchUID);
            if(onFill)
                onFill(fix::FillReport(aggressorUID, matchUID, qty, execPrice, Side::SELL));
        });
}

void LimitOrderBook::reduce(UID orderUID, Quantity quantity) {
    auto it = UIDtoOrderMap.find(orderUID);
    if(it == UIDtoOrderMap.end()) return;

    auto& order = it->second;
    if(quantity >= order->quantity) {
        cancel(orderUID);
        return;
    }

    order->quantity -= quantity;
    order->parentLimit->volumeAtLimit -= quantity;

    if(order->side == Side::BUY) 
        bids.volumeOfTree -= quantity;
    else 
        asks.volumeOfTree -= quantity;
}

void LimitOrderBook::cancel(UID orderUID) {
    auto it = UIDtoOrderMap.find(orderUID);
    if(it == UIDtoOrderMap.end()) return;

    auto& order = it->second;
    if(order->side == Side::BUY) 
        bids.cancel(order);
    else 
        asks.cancel(order);

    UIDtoOrderMap.erase(it);
}
void LimitOrderBook::clear() {
    asks.clear();
    bids.clear();
    UIDtoOrderMap.clear();
}





