#ifndef LIMIT_TREE_HPP
#define LIMIT_TREE_HPP

#include <map>
#include <memory>
#include <unordered_map>

#include "OrderStructures.hpp"

namespace LOB
{
    using PriceLimitMap = std::map<Price, std::shared_ptr<Limit>>;
    using PriceLimitExists = std::unordered_map<Price, std::shared_ptr<Limit>>;

    template <Side side>
    inline bool canMatch([[maybe_unused]] Price limitPrice, [[maybe_unused]] Price marketPrice) { return true; }

    template <>
    inline bool canMatch<Side::BUY>(Price limitPrice, Price marketPrice)
    {
        return marketPrice == 0 || limitPrice >= marketPrice;
    }

    template <>
    inline bool canMatch<Side::SELL>(Price limitPrice, Price marketPrice)
    {
        return marketPrice == 0 || limitPrice <= marketPrice;
    }

    // herb sutter on passing shared ptr by & and const & agrees
    // "Use a const shared_ptr& as a parameter only if you’re not sure whether or not you’ll take a copy and share ownership"
    template <Side side>
    inline void setBest([[maybe_unused]] std::shared_ptr<Limit> &best, [[maybe_unused]] const std::shared_ptr<Limit> &limit) {}

    template <>
    inline void setBest<Side::BUY>(std::shared_ptr<Limit> &bestBuy, const std::shared_ptr<Limit> &limit)
    {
        if (!bestBuy || (limit->priceAtLimit > bestBuy->priceAtLimit))
            bestBuy = limit;
    }

    template <>
    inline void setBest<Side::SELL>(std::shared_ptr<Limit> &bestSell, const std::shared_ptr<Limit> &limit)
    {
        if (!bestSell || (limit->priceAtLimit < bestSell->priceAtLimit))
            bestSell = limit;
    }

    template <Side side>
    inline void findBest([[maybe_unused]] std::shared_ptr<Limit> &best, [[maybe_unused]] PriceLimitMap &limits) {}

    template <>
    inline void findBest<Side::BUY>(std::shared_ptr<Limit> &best, PriceLimitMap &limits)
    {
        if (limits.size() == 1) // one price left at limit (we are removing the last)
        {
            best.reset();
            return;
        }

        // the next best buy limit is the one past the last one (last one is current) int the map because it is in sorted order
        auto nextBestIt = std::next(limits.rbegin());
        best = nextBestIt->second;
    }

    template <>
    inline void findBest<Side::SELL>(std::shared_ptr<Limit> &best, PriceLimitMap &limits)
    {
        if (limits.size() == 1) // one price left at limit (we are removing the last)
        {
            best.reset();
            return;
        }

        // the next sell buy limit is the first one int the map because it is in sorted order
        auto nextBestIt = std::next(limits.begin());
        best = nextBestIt->second;
    }

    template <Side side>
    struct LimitTree
    {
        PriceLimitMap limits;
        PriceLimitExists existingLimits;
        std::shared_ptr<Limit> best;
        Price lastBestPrice = 0;
        Count ordersInTree = 0;
        Volume volumeOfTree = 0;

        void clear()
        {
            limits.clear();
            existingLimits.clear();
            volumeOfTree = 0;
            ordersInTree = 0;
            best.reset();
        }

        void limit(Order* order)
        {
            auto it = existingLimits.find(order->price);
            std::shared_ptr<Limit> lim;

            if (it == existingLimits.end())
            {
                lim = std::make_shared<Limit>(order->price);
                setBest<side>(best, lim);
                limits.emplace(order->price, lim);
                existingLimits.emplace(order->price, lim);
            }
            else
            {
                lim = it->second;
            }

            order->parentLimit = lim;
            lim->push_back(order);
            ++ordersInTree;
            volumeOfTree += order->quantity;
        }

        void cancel(Order* order)
        {
            auto& lim = order->parentLimit;
            auto qty = order->quantity;

            // update head
            if(order->il_prev_)
                order->il_prev_->il_next_ = order->il_next_;
            else 
                lim->head_ = order->il_next_;
            
            // update tail
            if(order->il_next_)
                order->il_next_->il_prev_ = order->il_prev_;
            else 
                lim->tail_ = order->il_prev_;
            
            --lim->ordersAtLimit;
            lim->volumeAtLimit -= qty;

            if(lim->ordersAtLimit == 0) 
            {
                if(best == lim) findBest<side>(best, limits);
                limits.erase(order->price);
                existingLimits.erase(order->price);
            }

            --ordersInTree;
            volumeOfTree -= qty;
            if(best != nullptr)
                lastBestPrice = best->priceAtLimit;
        }

        template <typename Functor>
        void market(Order* order, Functor filledOrderWithUID)
        {
            while (best != nullptr && canMatch<side>(best->priceAtLimit, order->price))
            {
                Order* match = best->head_;
                UID matchUID = match->uid;
                Price execPrice = best->priceAtLimit;
                Quantity filledQty = std::min(match->quantity, order->quantity);

                if (match->quantity >= order->quantity)
                {
                    if (match->quantity == order->quantity)
                    {
                        cancel(match);
                        filledOrderWithUID(matchUID, order->uid, filledQty, execPrice, FillType::FULL);
                    }
                    else
                    {
                        match->quantity -= order->quantity;
                        match->parentLimit->volumeAtLimit -= order->quantity;
                        volumeOfTree -= order->quantity;
                        filledOrderWithUID(matchUID, order->uid, filledQty, execPrice, FillType::PARTIAL);
                    }
                    order->quantity = 0;
                    return;
                }
                order->quantity -= match->quantity;
                cancel(match);
                filledOrderWithUID(matchUID, order->uid, filledQty, execPrice, FillType::FULL);
            }
        }

        inline Volume volumeAt(Price price) const
        {
            auto it = limits.find(price);
            if (it != limits.end())
                return it->second->volumeAtLimit;
            return 0;
        }
        inline Count countAt(Price price)
        {
            if (limits.count(price))
                return limits.at(price)->ordersAtLimit;
            return 0;
        }
    };
}

#endif