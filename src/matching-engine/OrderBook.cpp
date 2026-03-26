#include "OrderBook.hpp"
using namespace LOB;

void LimitOrderBook::limit(Side side, UID orderUID, Quantity quantity, Price price, SessionId session_id)
{
    if (side == Side::BUY)
        return limitBuy(orderUID, quantity, price, session_id);
    else
        return limitSell(orderUID, quantity, price, session_id);
}

void LimitOrderBook::limitBuy(UID orderUID, Quantity quantity, Price price, SessionId session_id)
{
    Order* order = pool_.allocate();
    new (order) Order(orderUID, price, Side::BUY, quantity, session_id);
    UIDtoOrderMap.emplace(orderUID, order);

    if (asks.best != nullptr && price >= asks.best->priceAtLimit)
    {
        asks.market(order, [&](UID matchUID, UID aggressorUID, Quantity qty, Price execPrice, FillType fillType)
        { 
            SessionId restingSessionId = 0;
            auto restIt = UIDtoOrderMap.find(matchUID);

            if(restIt != UIDtoOrderMap.end()) 
            {
                restingSessionId = restIt->second->session_id;
                if(fillType == FillType::FULL) 
                {
                    Order* resting = restIt->second;
                    UIDtoOrderMap.erase(restIt);
                    resting->~Order();
                    pool_.deallocate(resting);
                }
            }

            if(fillOut_) 
            {
                ipc::FillEvent fe(aggressorUID, matchUID, qty, execPrice, Side::BUY, symbol_, session_id);
                fillOut_->tryPush(fe);
                if(restingSessionId != 0) {
                    fe.session_id_ = restingSessionId;
                    fillOut_->tryPush(fe);
                }
            }
        });
        if (order->quantity == 0)
        {
            UIDtoOrderMap.erase(orderUID);
            order->~Order();
            pool_.deallocate(order);
            return;
        }
    }
    bids.limit(order);
}

void LimitOrderBook::limitSell(UID orderUID, Quantity quantity, Price price, SessionId session_id)
{
    Order* order = pool_.allocate();
    new (order) Order(orderUID, price, Side::SELL, quantity, session_id);
    UIDtoOrderMap.emplace(orderUID, order);

    if (bids.best != nullptr && price <= bids.best->priceAtLimit)
    {
        bids.market(order, [&](UID matchUID, UID aggressorUID, Quantity qty, Price execPrice, FillType fillType)
        { 
            SessionId restingSessionId = 0;
            auto restIt = UIDtoOrderMap.find(matchUID);
            
            if(restIt != UIDtoOrderMap.end()) 
            {
                restingSessionId = restIt->second->session_id;
                if(fillType == FillType::FULL) 
                {
                    Order* resting = restIt->second;
                    UIDtoOrderMap.erase(restIt);
                    resting->~Order();
                    pool_.deallocate(resting);
                }
            }
            
            if (fillOut_) 
            {
                ipc::FillEvent fe(aggressorUID, matchUID, qty, execPrice, Side::SELL, symbol_, session_id);
                fillOut_->tryPush(fe);
                if (restingSessionId != 0) {
                    fe.session_id_ = restingSessionId;
                    fillOut_->tryPush(fe);
                }
            }
        });
        if (order->quantity == 0)
        {
            UIDtoOrderMap.erase(orderUID);
            order->~Order();
            pool_.deallocate(order);
            return;
        }
    }
    asks.limit(order);
}

void LimitOrderBook::market(Side side, UID orderUID, Quantity quantity, SessionId session_id)
{   
    if (side == Side::BUY)
        return marketBuy(orderUID, quantity, session_id);
    else
        return marketSell(orderUID, quantity, session_id);
}

void LimitOrderBook::marketBuy(UID orderUID, Quantity quantity, SessionId session_id)
{
    Order* order = pool_.allocate();
    new (order) Order(orderUID, 0, Side::BUY, quantity, session_id);

    asks.market(order, [&](UID matchUID, UID aggressorUID, Quantity qty, Price execPrice, FillType fillType)
        {
            SessionId restingSessionId = 0;
            auto restIt = UIDtoOrderMap.find(matchUID);
            
            if(restIt != UIDtoOrderMap.end()) 
            {
                restingSessionId = restIt->second->session_id;
                if(fillType == FillType::FULL) 
                {
                    Order* resting = restIt->second;
                    UIDtoOrderMap.erase(restIt);
                    resting->~Order();
                    pool_.deallocate(resting);
                }
            }

            if(fillOut_)
            {
                ipc::FillEvent fe(aggressorUID, matchUID, qty, execPrice, Side::BUY, symbol_, session_id);
                fillOut_->tryPush(fe);
                if (restingSessionId != 0) {
                    fe.session_id_ = restingSessionId;
                    fillOut_->tryPush(fe);
                }            
            }
        });

        order->~Order();
        pool_.deallocate(order);
}

void LimitOrderBook::marketSell(UID orderUID, Quantity quantity, SessionId session_id)
{
    Order* order = pool_.allocate();
    new (order) Order(orderUID, 0, Side::SELL, quantity, session_id);

    bids.market(order, [&](UID matchUID, UID aggressorUID, Quantity qty, Price execPrice, FillType fillType)
        {
            SessionId restingSessionId = 0;
            auto restIt = UIDtoOrderMap.find(matchUID);
            
            if(restIt != UIDtoOrderMap.end()) 
            {
                restingSessionId = restIt->second->session_id;
                if(fillType == FillType::FULL) 
                {
                    Order* resting = restIt->second;
                    UIDtoOrderMap.erase(restIt);
                    resting->~Order();
                    pool_.deallocate(resting);
                }
            }

            if(fillOut_)
            {
                ipc::FillEvent fe(aggressorUID, matchUID, qty, execPrice, Side::SELL, symbol_, session_id);
                fillOut_->tryPush(fe);
                if (restingSessionId != 0) {
                    fe.session_id_ = restingSessionId;
                    fillOut_->tryPush(fe);
                }            
            }        
        });

        order->~Order();
        pool_.deallocate(order);
}

void LimitOrderBook::reduce(UID orderUID, Quantity quantity) {
    auto it = UIDtoOrderMap.find(orderUID);
    if(it == UIDtoOrderMap.end()) return;

    auto* order = it->second;
    if(quantity >= order->quantity) 
    {
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

    auto* order = it->second;
    if(order->side == Side::BUY) 
        bids.cancel(order);
    else 
        asks.cancel(order);

    UIDtoOrderMap.erase(it);
    order->~Order();
    pool_.deallocate(order);
}
void LimitOrderBook::clear() {
    for(auto& [uid, order] : UIDtoOrderMap)
    {
        order->~Order();
        pool_.deallocate(order);
    }
    UIDtoOrderMap.clear();
    asks.clear();
    bids.clear();
}





