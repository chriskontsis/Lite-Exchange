#ifndef LIMIT_ORDER_BOOK_HPP
#define LIMIT_ORDER_BOOK_HPP

#include "../ipc/FillEvent.hpp"
#include "../ipc/MPSC_Queue.hpp"
#include "LimitTree.hpp"
#include "OrderPool.hpp"
#include "OrderStructures.hpp"
#include "absl/container/flat_hash_map.h"

namespace LOB
{
using UIDOrderMap = absl::flat_hash_map<UID, Order*>;

class LimitOrderBook
{
 private:
  OrderPool<65536>                   pool_;
  LimitTree<Side::SELL>              asks_;
  LimitTree<Side::BUY>               bids_;
  UIDOrderMap                        uid_to_order_map_;
  MPSC_Queue<ipc::FillEvent, 65536>* fill_out_{nullptr};
  SymbolId                           symbol_id_{0};

  void limitSell(UID order_uid, Quantity quantity, Price price, SessionId session_id,
                 TimeInForce tif = TimeInForce::GTC);
  void limitBuy(UID order_uid, Quantity quantity, Price price, SessionId session_id,
                TimeInForce tif = TimeInForce::GTC);
  void marketBuy(UID order_uid, Quantity quantity, SessionId session_id);
  void marketSell(UID order_uid, Quantity quantity, SessionId session_id);

 public:
  LimitOrderBook() = default;
  LimitOrderBook(MPSC_Queue<ipc::FillEvent, 65536>& fill_out, SymbolId symbol_id)
      : fill_out_(&fill_out), symbol_id_(symbol_id)
  {
  }

  void clear();
  void limit(Side side, UID order_uid, Quantity quantity, Price price, SessionId session_id = 0,
             TimeInForce tif = TimeInForce::GTC);
  void market(Side side, UID order_uid, Quantity quantity, SessionId session_id = 0);
  void reduce(UID order_uid, Quantity quantity);
  void cancel(UID order_uid);

  Price bestBid() const { return bids_.best_ ? bids_.best_->price_at_limit_ : 0; }
  Price bestAsk() const { return asks_.best_ ? asks_.best_->price_at_limit_ : 0; }
  Quantity volumeAt(Side side, Price price) const
  {
    return (side == Side::BUY) ? bids_.volumeAt(price) : asks_.volumeAt(price);
  }
  bool hasOrder(UID uid) const { return uid_to_order_map_.contains(uid); }
};
};  // namespace LOB

#endif
