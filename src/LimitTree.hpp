#ifndef LIMIT_TREE_HPP
#define LIMIT_TREE_HPP

#include "Structures.hpp"
#include <map>
#include <iostream>

namespace LOB
{

    using Count = u_int32_t;
    using PriceLimitMap = std::map<Price, Limit *>;

    template <Side side>
    inline bool canMatch([[maybe_unused]] Price limit, [[maybe_unused]] Price market) { return true; }

    template <>
    inline bool canMatch<Side::BUY>(Price limit, Price market)
    {
        return market == 0 || market <= limit;
    }

    template <>
    inline bool canMatch<Side::SELL>(Price limit, Price market)
    {
        return market == 0 || market >= limit;
    }

    /* Template for setting best limit for each side*/
    template <Side side>
    void setBest([[maybe_unused]] Limit **best, [[maybe_unused]] Limit *limt) {}

    template <>
    inline void setBest<Side::BUY>(Limit **highestBuy, Limit *limit)
    {
        if (*highestBuy == nullptr)
            *highestBuy = limit;
        else if (limit->price > (*highestBuy)->price)
            *highestBuy = limit;
    }

    template <>
    inline void setBest<Side::SELL>(Limit **lowestSell, Limit *limit)
    {
        if (*lowestSell == nullptr)
            *lowestSell = limit;
        else if (limit->price < (*lowestSell)->price)
            *lowestSell = limit;
    }

    /*
    Template for finding best limit for each side
    */
    template <Side side>
    void findBest([[maybe_unused]] Limit **highestBuy, [[maybe_unused]] PriceLimitMap &limits) {}

    template <>
    inline void findBest<Side::BUY>(Limit **highestBuy, PriceLimitMap &limits)
    {
        if (limits.size() == 1) // last limit were removing
        {
            *highestBuy = nullptr;
            return;
        }

        auto it = std::next(limits.rbegin());
        *highestBuy = it->second;
    }

    template <>
    inline void findBest<Side::SELL>(Limit **lowestSell, PriceLimitMap &limits)
    {
        if (limits.size() == 1) // last limit we are removing
        {
            *lowestSell = nullptr;
            return;
        }
        auto it = std::next(limits.begin());
        *lowestSell = it->second;
    }

    template <Side side>
    class LimitTree
    {
    public:
        Volume volumeOfOrdersInTree{0};
        Count countOrdersInTree{0};
        PriceLimitMap limits;
        Limit *best = nullptr;
        Price lastBestPrice{0};

        void clear()
        {
            for (auto it = limits.begin(); it != limits.end(); ++it)
            {
                it->second->ordersList.clear();
            }
            limits.clear();
            volumeOfOrdersInTree = 0;
            countOrdersInTree = 0;
        }

        void limit(Order &order)
        {
            if (limits.count(order.price) == 0)
            {
                order.limit = new Limit(order);
                setBest<side>(&best, order.limit);
                limits.emplace(order.price, order.limit);
                limits[order.price]->ordersList.emplace_back(order);
                limits[order.price]->orderIterators[order.uid] = std::prev(limits[order.price]->ordersList.end());
            }
            else
            {
                order.limit = limits.at(order.price);
                ++order.limit->countOrdersAtLimit;
                order.limit->volume += order.quantity;
                limits[order.price]->ordersList.emplace_back(order);
                limits[order.price]->orderIterators[order.uid] = std::prev(limits[order.price]->ordersList.end());
            }
            ++countOrdersInTree;
            volumeOfOrdersInTree += order.quantity;
            if(best != nullptr) lastBestPrice = best->price;
        }


        void cancel(Order &order)
        {
            auto &orderList = order.limit->ordersList;
            auto limit_ = order.limit;
            auto qty = order.quantity;

            if (orderList.size() == 1) // last order at limit
            {
                if (best == limit_) {
                    findBest<side>(&best, limits);
                }
                limits.erase(order.price);
                delete limit_;
            }
            else
            {
                --limit_->countOrdersAtLimit;
                limit_->volume -= order.quantity;
                auto orderIterator = limit_->orderIterators[order.uid];
                limit_->orderIterators.erase(order.uid);
                limit_->ordersList.erase(orderIterator);
            }
            --countOrdersInTree;
            volumeOfOrdersInTree -= qty;
            if (best != nullptr)
                lastBestPrice = best->price;
        }

        template <typename Callback>
        void market(Order &order, Callback filledOrderWithUID)
        {
            while (best != nullptr && canMatch<side>(best->price, order.price))
            {

                auto &match = best->ordersList.front();
                if (match.quantity >= order.quantity)
                {
                    if (match.quantity == order.quantity)
                    {
                        cancel(match);
                        filledOrderWithUID(order.uid);
                    }
                    else
                    {
                        match.quantity -= order.quantity;
                        match.limit->volume -= order.quantity;
                        volumeOfOrdersInTree -= order.quantity;
                    }
                    order.quantity = 0;
                    return;
                }
                order.quantity -= match.quantity;
                cancel(match);
                filledOrderWithUID(match.uid);
            }
        }

        inline Volume volumeAt(Price price)
        {
            if (limits.count(price))
                return limits[price]->volume;
            return 0;
        }
        inline Count countAt(Price price)
        {
            if (limits.count(price))
                return limits.at(price)->countOrdersAtLimit;
            return 0;
        }
    };
}

#endif