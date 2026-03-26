#ifndef LIMIT_TREE_HPP
#define LIMIT_TREE_HPP

#include <array>
#include "OrderStructures.hpp"

namespace LOB
{
    static constexpr Price kBase = 1;
    static constexpr size_t kMaxLevels = 16384;

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

    template <Side side>
    inline void setBest([[maybe_unused]] Limit*& best, [[maybe_unused]] Limit* lim)  {}

    template <>
    inline void setBest<Side::BUY>(Limit*& best, Limit* lim)
    {
        if (!best || lim->priceAtLimit > best->priceAtLimit)
            best = lim;
    }

    template <>
    inline void setBest<Side::SELL>(Limit*& best, Limit* lim)
    {
        if (!best || lim->priceAtLimit < best->priceAtLimit)
            best = lim;
    }

    template <Side side>
    inline void findBest([[maybe_unused]] Limit*& best, 
                         [[maybe_unused]] std::array<Limit, kMaxLevels>& slots, 
                         [[maybe_unused]] size_t exhausted_idx) 
                        { }

    template <>
    inline void findBest<Side::BUY>(Limit*& best,
                std::array<Limit, kMaxLevels>& slots, size_t exhausted_idx)
    {
        for(size_t i = exhausted_idx; i-- > 0;)
        {
            if(slots[i].ordersAtLimit > 0) { best = &slots[i]; return; }
        }
        best = nullptr;
    }

    template <>
    inline void findBest<Side::SELL>(Limit*& best,
                std::array<Limit, kMaxLevels>& slots, size_t exhausted_idx)
    {
        for(size_t i = exhausted_idx+1; i < kMaxLevels; ++i)
        {
            if(slots[i].ordersAtLimit > 0) { best = &slots[i]; return; }
        }
        best = nullptr;
    }

    template <Side side>
    struct LimitTree
    {
        std::array<Limit, kMaxLevels> levels_ { };
        Limit* best { nullptr };
        Count ordersInTree { 0 };
        Volume volumeOfTree { 0 };

        void clear()
        {
            for(auto& level : levels_)
            {
                level.head_ = nullptr;
                level.tail_ = nullptr;
                level.ordersAtLimit = 0;
                level.volumeAtLimit = 0;
            }
            best = nullptr;
            ordersInTree = 0;
            volumeOfTree = 0;
        }

        void limit(Order* order)
        {
            size_t idx = priceToLevel(order->price);
            Limit& lim = levels_[idx];

            if (lim.ordersAtLimit == 0)
            {
                lim.priceAtLimit = order->price;
                setBest<side>(best, &lim);
            }

            order->parentLimit = &lim;
            lim.push_back(order);
            ++ordersInTree;
            volumeOfTree += order->quantity;
        }

        void cancel(Order* order)
        {
            Limit* lim = order->parentLimit;
            Quantity qty = order->quantity;

            if(order->il_prev_) 
                order->il_prev_->il_next_ = order->il_next_;
            else 
                lim->head_ = order->il_next_;

            if(order->il_next_)
                order->il_next_->il_prev_ = order->il_prev_;
            else 
                lim->tail_ = order->il_prev_;

            --lim->ordersAtLimit;
            lim->volumeAtLimit -= qty;

            if(lim->ordersAtLimit == 0 && lim == best)
            {
                size_t idx = priceToLevel(lim->priceAtLimit);
                findBest<side>(best, levels_, idx);
            }

            --ordersInTree;
            volumeOfTree -= qty;
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
            size_t idx = priceToLevel(price);
            if(idx >= kMaxLevels) return 0;
            return levels_[idx].volumeAtLimit;
        }
        inline Count countAt(Price price) const 
        {
            size_t idx = priceToLevel(price);
            if(idx >= kMaxLevels) return 0;
            return levels_[idx].ordersAtLimit;
        }

        static size_t priceToLevel(Price p) { return static_cast<size_t>(p - kBase); }
    };
}

#endif