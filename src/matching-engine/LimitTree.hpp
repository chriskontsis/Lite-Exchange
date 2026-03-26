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
    inline void setBest([[maybe_unused]] Limit *&best, [[maybe_unused]] Limit *lim) {}

    template <>
    inline void setBest<Side::BUY>(Limit *&best, Limit *lim)
    {
        if (!best || lim->priceAtLimit > best->priceAtLimit)
            best = lim;
    }

    template <>
    inline void setBest<Side::SELL>(Limit *&best, Limit *lim)
    {
        if (!best || lim->priceAtLimit < best->priceAtLimit)
            best = lim;
    }

    template <Side side>
    struct LimitTree
    {
        std::array<Limit, kMaxLevels> levels_{};
        Limit *best{nullptr};
        Count ordersInTree{0};
        Volume volumeOfTree{0};

        void clear()
        {
            for (auto &level : levels_)
            {
                level.head_ = nullptr;
                level.tail_ = nullptr;
                level.ordersAtLimit = 0;
                level.volumeAtLimit = 0;
                level.occ_next_ = nullptr;
                level.occ_prev_ = nullptr;
            }
            best = nullptr;
            ordersInTree = 0;
            volumeOfTree = 0;
        }

        void limit(Order *order)
        {
            size_t idx = priceToLevel(order->price);
            Limit &lim = levels_[idx];

            if (lim.ordersAtLimit == 0)
            {
                lim.priceAtLimit = order->price;
                insertOccupied(idx, lim);
            }

            order->parentLimit = &lim;
            lim.push_back(order);
            ++ordersInTree;
            volumeOfTree += order->quantity;
        }

        void cancel(Order *order)
        {
            Limit *lim = order->parentLimit;
            Quantity qty = order->quantity;

            if (order->il_prev_)
                order->il_prev_->il_next_ = order->il_next_;
            else
                lim->head_ = order->il_next_;

            if (order->il_next_)
                order->il_next_->il_prev_ = order->il_prev_;
            else
                lim->tail_ = order->il_prev_;

            --lim->ordersAtLimit;
            lim->volumeAtLimit -= qty;

            if (lim->ordersAtLimit == 0)
            {
                removeOccupied(lim);
            }

            --ordersInTree;
            volumeOfTree -= qty;
        }

        template <typename Functor>
        void market(Order *order, Functor filledOrderWithUID)
        {
            while (best != nullptr && canMatch<side>(best->priceAtLimit, order->price))
            {
                Order *match = best->head_;
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
            if (idx >= kMaxLevels)
                return 0;
            return levels_[idx].volumeAtLimit;
        }
        inline Count countAt(Price price) const
        {
            size_t idx = priceToLevel(price);
            if (idx >= kMaxLevels)
                return 0;
            return levels_[idx].ordersAtLimit;
        }

        static size_t priceToLevel(Price p) { return static_cast<size_t>(p - kBase); }

    private:
        void insertOccupied(size_t idx, Limit& lim)
        {
            Limit* prev = nullptr;
            for(size_t i = idx; i-- > 0;)
            {
                if(levels_[i].ordersAtLimit > 0) { prev = &levels_[i]; break; }
            }

            Limit* next = nullptr;
            for(size_t i = idx+1; i < kMaxLevels; ++i)
            {
                if(levels_[i].ordersAtLimit > 0) { next = &levels_[i]; break; }
            }

            lim.occ_prev_ = prev;
            lim.occ_next_ = next;
            if(prev) prev->occ_next_ = &lim;
            if(next) next->occ_prev_ = &lim;

            setBest<side>(best, &lim);
        }

        void removeOccupied(Limit* lim)
        {
            if(lim == best)
            {
                if constexpr (side == Side::BUY)
                    best = lim->occ_prev_;
                else 
                    best = lim->occ_next_;
            }

            if(lim->occ_prev_) lim->occ_prev_->occ_next_ = lim->occ_next_;
            if(lim->occ_next_) lim->occ_next_->occ_prev_ = lim->occ_prev_;
            lim->occ_prev_ = nullptr;
            lim->occ_next_ = nullptr;
        }
    };
}

#endif