#ifndef ORDER_STRUCTURES_HPP
#define ORDER_STRUCTURES_HPP

#include <cstdint>

namespace LOB
{

    using UID = uint64_t;
    using Quantity = uint32_t;
    using Price = uint64_t;
    using Count = uint32_t;
    using Volume = uint64_t;
    using SessionId = uint32_t;

    class Limit;
    enum class Side
    {
        BUY,
        SELL
    };

    enum class FillType { FULL, PARTIAL };


    struct Order 
    {
        const UID uid { 0 };
        const Price price { 0 };
        Quantity quantity { 0 };
        Side side;
        Limit* parentLimit { nullptr };
        SessionId session_id { 0 };
        Order* il_prev_ { nullptr };
        Order* il_next_ { nullptr };

        Order(UID uid_, Price price_, Side side_, Quantity qty_, SessionId sid = 0) :
        uid(uid_), price(price_), quantity(qty_), side(side_), session_id {sid} 
        { }
    };

    struct Limit
    {
        Price priceAtLimit { 0 };
        Volume volumeAtLimit { 0 };
        Count ordersAtLimit { 0 };
        Order* head_ { nullptr };
        Order* tail_ { nullptr };
        Limit* occ_prev_ { nullptr };
        Limit* occ_next_ { nullptr };

        Limit() = default;
        explicit Limit(Price p) : priceAtLimit(p) { }

        void push_back(Order* order)
        {
            order->il_prev_ = tail_;
            order->il_next_ = nullptr;
            if(tail_) tail_->il_next_ = order;
            else head_ = order;
            tail_ = order;
            ++ordersAtLimit;
            volumeAtLimit += order->quantity;
        }
    };



}

#endif