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

  enum class FillType
  {
    FULL,
    PARTIAL
  };

  struct Order
  {
    const UID      uid_ {0};
    const Price    price_ {0};
    Quantity       quantity_ {0};
    Side           side_;
    Limit*         parent_limit_ {nullptr};
    SessionId      session_id_ {0};
    Order*         il_prev_ {nullptr};
    Order*         il_next_ {nullptr};

    Order(UID uid, Price price, Side side, Quantity qty, SessionId sid = 0)
        : uid_(uid), price_(price), quantity_(qty), side_(side), session_id_(sid)
    {
    }
  };

  struct Limit
  {
    Price   price_at_limit_ {0};
    Volume  volume_at_limit_ {0};
    Count   orders_at_limit_ {0};
    Order*  head_ {nullptr};
    Order*  tail_ {nullptr};
    Limit*  occ_prev_ {nullptr};
    Limit*  occ_next_ {nullptr};

    Limit() = default;
    explicit Limit(Price p) : price_at_limit_(p) {}

    void push_back(Order* order)
    {
      order->il_prev_ = tail_;
      order->il_next_ = nullptr;
      if (tail_)
        tail_->il_next_ = order;
      else
        head_ = order;
      tail_ = order;
      ++orders_at_limit_;
      volume_at_limit_ += order->quantity_;
    }
  };

}

#endif
