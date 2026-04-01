#ifndef LIMIT_TREE_HPP
#define LIMIT_TREE_HPP

#include <array>

#include "OrderStructures.hpp"

namespace LOB
{
static constexpr Price  kBase = 1;
static constexpr size_t kMaxLevels = 16384;

template <Side side>
inline bool canMatch([[maybe_unused]] Price limit_price, [[maybe_unused]] Price market_price)
{
  return true;
}

template <>
inline bool canMatch<Side::BUY>(Price limit_price, Price market_price)
{
  return market_price == 0 || limit_price >= market_price;
}

template <>
inline bool canMatch<Side::SELL>(Price limit_price, Price market_price)
{
  return market_price == 0 || limit_price <= market_price;
}

template <Side side>
inline void setBest([[maybe_unused]] Limit*& best, [[maybe_unused]] Limit* lim)
{
}

template <>
inline void setBest<Side::BUY>(Limit*& best, Limit* lim)
{
  if (!best || lim->price_at_limit_ > best->price_at_limit_)
    best = lim;
}

template <>
inline void setBest<Side::SELL>(Limit*& best, Limit* lim)
{
  if (!best || lim->price_at_limit_ < best->price_at_limit_)
    best = lim;
}

template <Side side>
struct LimitTree
{
  std::array<Limit, kMaxLevels> levels_{};
  Limit*                        best_{nullptr};
  Count                         orders_in_tree_{0};
  Volume                        volume_of_tree_{0};

  void clear()
  {
    for (auto& level : levels_)
    {
      level.head_ = nullptr;
      level.tail_ = nullptr;
      level.orders_at_limit_ = 0;
      level.volume_at_limit_ = 0;
      level.occ_next_ = nullptr;
      level.occ_prev_ = nullptr;
    }
    best_ = nullptr;
    orders_in_tree_ = 0;
    volume_of_tree_ = 0;
  }

  void limit(Order* order)
  {
    size_t idx = priceToLevel(order->price_);
    Limit& lim = levels_[idx];

    if (lim.orders_at_limit_ == 0)
    {
      lim.price_at_limit_ = order->price_;
      insertOccupied(idx, lim);
    }

    order->parent_limit_ = &lim;
    lim.push_back(order);
    ++orders_in_tree_;
    volume_of_tree_ += order->quantity_;
  }

  void cancel(Order* order)
  {
    Limit*   lim = order->parent_limit_;
    Quantity qty = order->quantity_;

    if (order->il_prev_)
      order->il_prev_->il_next_ = order->il_next_;
    else
      lim->head_ = order->il_next_;

    if (order->il_next_)
      order->il_next_->il_prev_ = order->il_prev_;
    else
      lim->tail_ = order->il_prev_;

    --lim->orders_at_limit_;
    lim->volume_at_limit_ -= qty;

    if (lim->orders_at_limit_ == 0)
      removeOccupied(lim);

    --orders_in_tree_;
    volume_of_tree_ -= qty;
  }

  template <typename Functor>
  void market(Order* order, Functor filledOrderWithUID)
  {
    while (best_ != nullptr && canMatch<side>(best_->price_at_limit_, order->price_))
    {
      Order*   match = best_->head_;
      UID      match_uid = match->uid_;
      Price    exec_price = best_->price_at_limit_;
      Quantity filled_qty = std::min(match->quantity_, order->quantity_);

      if (match->quantity_ >= order->quantity_)
      {
        if (match->quantity_ == order->quantity_)
        {
          cancel(match);
          filledOrderWithUID(match_uid, order->uid_, filled_qty, exec_price, FillType::FULL);
        }
        else
        {
          match->quantity_ -= order->quantity_;
          match->parent_limit_->volume_at_limit_ -= order->quantity_;
          volume_of_tree_ -= order->quantity_;
          filledOrderWithUID(match_uid, order->uid_, filled_qty, exec_price, FillType::PARTIAL);
        }
        order->quantity_ = 0;
        return;
      }
      order->quantity_ -= match->quantity_;
      cancel(match);
      filledOrderWithUID(match_uid, order->uid_, filled_qty, exec_price, FillType::FULL);
    }
  }

  inline Volume volumeAt(Price price) const
  {
    size_t idx = priceToLevel(price);
    if (idx >= kMaxLevels)
      return 0;
    return levels_[idx].volume_at_limit_;
  }

  inline Count countAt(Price price) const
  {
    size_t idx = priceToLevel(price);
    if (idx >= kMaxLevels)
      return 0;
    return levels_[idx].orders_at_limit_;
  }

  static size_t priceToLevel(Price p) { return static_cast<size_t>(p - kBase); }

 private:
  void insertOccupied(size_t idx, Limit& lim)
  {
    Limit* prev = nullptr;
    for (size_t i = idx; i-- > 0;)
    {
      if (levels_[i].orders_at_limit_ > 0)
      {
        prev = &levels_[i];
        break;
      }
    }

    Limit* next = nullptr;
    for (size_t i = idx + 1; i < kMaxLevels; ++i)
    {
      if (levels_[i].orders_at_limit_ > 0)
      {
        next = &levels_[i];
        break;
      }
    }

    lim.occ_prev_ = prev;
    lim.occ_next_ = next;
    if (prev)
      prev->occ_next_ = &lim;
    if (next)
      next->occ_prev_ = &lim;

    setBest<side>(best_, &lim);
  }

  void removeOccupied(Limit* lim)
  {
    if (lim == best_)
    {
      if constexpr (side == Side::BUY)
        best_ = lim->occ_prev_;
      else
        best_ = lim->occ_next_;
    }

    if (lim->occ_prev_)
      lim->occ_prev_->occ_next_ = lim->occ_next_;
    if (lim->occ_next_)
      lim->occ_next_->occ_prev_ = lim->occ_prev_;
    lim->occ_prev_ = nullptr;
    lim->occ_next_ = nullptr;
  }
};
}  // namespace LOB

#endif
