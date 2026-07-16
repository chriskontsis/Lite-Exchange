#pragma once
#include <algorithm>
#include <climits>
#include <cstdint>

#include "lx/book/order.hpp"
#include "lx/book/pool.hpp"
#include "lx/book/price_level.hpp"
#include "lx/proto/messages.hpp"

namespace lx::book
{

struct OrderHandle
{
  static constexpr uint32_t INVALID = UINT32_MAX;
  uint32_t                  slot = INVALID;
  uint32_t                  gen = 0;
  bool valid() const { return slot != INVALID; }
};
template <uint32_t MAX_ORDERS, uint32_t LADDER_SIZE>
class OrderBook
{
 public:
  OrderBook(int64_t base_price, int64_t tick_size)
      : base_price_(base_price),
        tick_size_(tick_size),
        best_bid_idx_(NULL_IDX),
        best_ask_idx_(NULL_IDX)
  {
  }

  // Returns a valid OrderHandle if the order rests; invalid handle if filled or error.
  OrderHandle add_order(const proto::NewOrder& msg, proto::Fill* fills, uint32_t& fill_count,
                        uint32_t max_fills)
  {
    int32_t  idx = price_to_idx(msg.price);
    uint32_t uidx = static_cast<uint32_t>(idx);
    if (idx < 0 || uidx >= LADDER_SIZE)
      return {};

    uint32_t slot = pool_.alloc();
    if (slot == Pool<Order, MAX_ORDERS>::INVALID_IDX)
      return {};

    Order& o = pool_[slot];
    o.order_id = msg.order_id;
    o.price = msg.price;
    o.qty = msg.qty;
    o.symbol = msg.symbol_id;
    o.side = msg.side;
    o.tif = msg.tif;
    o.next_idx = NULL_IDX;
    o.prev_idx = NULL_IDX;

    fill_count = 0;

    if (msg.side == proto::Side::BUY)
    {
      match<proto::Side::BUY>(o, uidx, fills, fill_count, max_fills);
      if (o.qty > 0 && msg.tif != proto::TimeInForce::IOC)
      {
        bid_levels_[uidx].push_back(slot, pool_.data());
        if (best_bid_idx_ == NULL_IDX || uidx > best_bid_idx_)
          best_bid_idx_ = uidx;
        return {slot, pool_.gen(slot)};
      }
    }
    else
    {
      match<proto::Side::SELL>(o, uidx, fills, fill_count, max_fills);
      if (o.qty > 0 && msg.tif != proto::TimeInForce::IOC)
      {
        ask_levels_[uidx].push_back(slot, pool_.data());
        if (best_ask_idx_ == NULL_IDX || uidx < best_ask_idx_)
          best_ask_idx_ = uidx;
        return {slot, pool_.gen(slot)};
      }
    }

    pool_.free(slot);
    return {};
  }

  bool cancel_order(OrderHandle h)
  {
    if (h.slot >= MAX_ORDERS || pool_.gen(h.slot) != h.gen)
      return false;

    Order&   o = pool_[h.slot];
    uint32_t uidx = static_cast<uint32_t>(price_to_idx(o.price));

    if (o.side == proto::Side::BUY)
    {
      bid_levels_[uidx].remove(h.slot, pool_.data());
      if (bid_levels_[uidx].empty() && best_bid_idx_ == uidx)
        best_bid_idx_ = find_best<proto::Side::BUY>(uidx > 0 ? uidx - 1 : NULL_IDX);
    }
    else
    {
      ask_levels_[uidx].remove(h.slot, pool_.data());
      if (ask_levels_[uidx].empty() && best_ask_idx_ == uidx)
        best_ask_idx_ = find_best<proto::Side::SELL>(uidx + 1);
    }

    pool_.free(h.slot);
    return true;
  }

  int64_t best_bid_price() const
  {
    if (best_bid_idx_ == NULL_IDX)
      return INT64_MIN;
    return base_price_ + static_cast<int64_t>(best_bid_idx_) * tick_size_;
  }

  int64_t best_ask_price() const
  {
    if (best_ask_idx_ == NULL_IDX)
      return INT64_MAX;
    return base_price_ + static_cast<int64_t>(best_ask_idx_) * tick_size_;
  }

 private:
  int64_t  base_price_;
  int64_t  tick_size_;
  uint32_t best_bid_idx_;
  uint32_t best_ask_idx_;

  PriceLevel              bid_levels_[LADDER_SIZE];
  PriceLevel              ask_levels_[LADDER_SIZE];
  Pool<Order, MAX_ORDERS> pool_;

  int32_t price_to_idx(int64_t price) const
  {
    return static_cast<int32_t>((price - base_price_) / tick_size_);
  }

  template <proto::Side Side>
  uint32_t find_best(uint32_t from)
  {
    if constexpr (Side == proto::Side::BUY)
    {
      for (uint32_t i = from; i != NULL_IDX; --i)
      {
        if (!bid_levels_[i].empty())
          return i;
        if (i == 0)
          break;
      }
    }
    else
    {
      for (uint32_t i = from; i < LADDER_SIZE; ++i)
      {
        if (!ask_levels_[i].empty())
          return i;
      }
    }

    return NULL_IDX;
  }

  template <proto::Side Side>
  void match(Order& aggressor, uint32_t agg_idx, proto::Fill* fills, uint32_t& fill_count,
             uint32_t max_fills)
  {
    uint32_t&   best = (Side == proto::Side::BUY ? best_ask_idx_ : best_bid_idx_);
    PriceLevel* levels = (Side == proto::Side::BUY ? ask_levels_ : bid_levels_);

    while (aggressor.qty > 0 && best != NULL_IDX)
    {
      if constexpr (Side == proto::Side::BUY)
      {
        if (best > agg_idx)
          break;
      }
      else
      {
        if (best < agg_idx)
          break;
      }

      PriceLevel& level = levels[best];
      while (aggressor.qty > 0 && !level.empty())
      {
        uint32_t rslot = level.head_idx;
        Order&   resting = pool_[rslot];
        uint32_t fill_qty = std::min(aggressor.qty, resting.qty);

        if (fill_count >= max_fills)
          return;
        fills[fill_count++] = proto::Fill{{sizeof(proto::Fill), proto::MsgType::FILL, 0},
                                          fill_qty,
                                          aggressor.order_id,
                                          resting.order_id,
                                          resting.price};
        level.total_qty -= fill_qty;
        aggressor.qty -= fill_qty;
        resting.qty -= fill_qty;

        if (resting.qty == 0)
        {
          level.pop_front(pool_.data());
          pool_.free(rslot);
        }
      }

      if (level.empty())
      {
        if constexpr (Side == proto::Side::BUY)
          best = find_best<proto::Side::SELL>(best + 1);
        else
          best = best > 0 ? find_best<proto::Side::BUY>(best - 1) : NULL_IDX;
      }
    }
  }
};
}  // namespace lx::book