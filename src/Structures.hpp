#ifndef STRUCTURES_HPP
#define STRUCTURES_HPP

#include <list>
#include <cstdint>
#include <unordered_map>

namespace LOB
{

    using Price = u_int64_t;
    using UID = u_int64_t;
    using Quantity = u_int64_t;
    using Volume = u_int64_t;

    class Limit;
    enum class Side
    {
        BUY,
        SELL
    };

    class Order
    {
    public:
        const UID uid{0};
        const Price price;
        Quantity quantity{0};
        const Side side;
        Limit *limit;

        Order(UID uid_, Price price_, Quantity quantity_, Side side_) : uid(uid_), price(price_),
                                                                        quantity(quantity_), side(side_) {}
    };

    class Limit
    {
    public:
        Volume volume{0};
        u_int32_t countOrdersAtLimit{0};
        Price price;
        std::list<Order> ordersList;
        std::unordered_map<int, std::list<Order>::iterator> orderIterators;

        Limit(Order &order) : volume(order.quantity), countOrdersAtLimit(1), price(order.price)
        {
        }
        Limit(Order &&order) : volume(order.quantity), countOrdersAtLimit(1), price(order.price)
        {
        }
    };
}
#endif