#ifndef ORDER_STRUCTURES_HPP
#define ORDER_STRUCTURES_HPP

#include <cstdint>
#include <list>
#include <memory>
#include <unordered_map>

namespace LOB
{

    using UID = u_int64_t;
    using Quantity = u_int32_t;
    using Price = u_int64_t;
    using Count = u_int32_t;
    using Volume = u_int64_t;
    using SessionId = u_int32_t;

    class Limit;
    enum class Side
    {
        BUY,
        SELL
    };

    enum class FillType { FULL, PARTIAL };


    struct Order 
    {
        const UID uid {0};
        const Price price {0};
        Quantity quantity {0};
        Side side;
        std::shared_ptr<Limit> parentLimit;
        SessionId session_id {0};

        Order(UID uid_, Price price_, Side side_, Quantity qty_, SessionId sid = 0) :
        uid(uid_), price(price_), quantity(qty_), side(side_), session_id {sid} 
        { }
    };


    using OrderPositionIterators = std::list<std::shared_ptr<Order>>::iterator;
    using OrderPositionMap = std::unordered_map<UID, OrderPositionIterators>;


    struct Limit
    {
        Price priceAtLimit;
        Volume volumeAtLimit;
        Count ordersAtLimit;
        std::list<std::shared_ptr<Order>> orderList;
        OrderPositionMap orderPositions;

        Limit(Order* order) : priceAtLimit(order->price), volumeAtLimit(order->quantity), ordersAtLimit(1)
        {}

    };



}

#endif