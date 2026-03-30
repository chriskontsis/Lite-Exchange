#include "OrderBook.hpp"

using namespace LOB;

void LimitOrderBook::limit(Side side, UID order_uid, Quantity quantity, Price price,
                           SessionId session_id, TimeInForce tif)
{
  if (side == Side::BUY)
    return limitBuy(order_uid, quantity, price, session_id, tif);
  else
    return limitSell(order_uid, quantity, price, session_id, tif);
}

void LimitOrderBook::limitBuy(UID order_uid, Quantity quantity, Price price, SessionId session_id,
                              TimeInForce tif)
{
  Order* order = pool_.allocate();
  new (order) Order(order_uid, price, Side::BUY, quantity, session_id);
  uid_to_order_map_.emplace(order_uid, order);

  if (asks_.best_ != nullptr && price >= asks_.best_->price_at_limit_)
  {
    asks_.market(
        order,
        [&](UID match_uid, UID aggressor_uid, Quantity qty, Price exec_price, FillType fill_type)
        {
          SessionId resting_session_id = 0;
          auto      rest_it = uid_to_order_map_.find(match_uid);

          if (rest_it != uid_to_order_map_.end())
          {
            resting_session_id = rest_it->second->session_id_;
            if (fill_type == FillType::FULL)
            {
              Order* resting = rest_it->second;
              uid_to_order_map_.erase(rest_it);
              resting->~Order();
              pool_.deallocate(resting);
            }
          }

          if (fill_out_)
          {
            ipc::FillEvent fe(aggressor_uid, match_uid, qty, exec_price, Side::BUY, session_id,
                              symbol_id_);
            fill_out_->tryPush(fe);
            if (resting_session_id != 0)
            {
              fe.session_id_ = resting_session_id;
              fill_out_->tryPush(fe);
            }
          }
        });
    if (order->quantity_ == 0)
    {
      uid_to_order_map_.erase(order_uid);
      order->~Order();
      pool_.deallocate(order);
      return;
    }
  }

  if (tif == TimeInForce::IOC)
  {
    uid_to_order_map_.erase(order_uid);
    order->~Order();
    pool_.deallocate(order);
    return;
  }

  bids_.limit(order);
}

void LimitOrderBook::limitSell(UID order_uid, Quantity quantity, Price price, SessionId session_id,
                               TimeInForce tif)
{
  Order* order = pool_.allocate();
  new (order) Order(order_uid, price, Side::SELL, quantity, session_id);
  uid_to_order_map_.emplace(order_uid, order);

  if (bids_.best_ != nullptr && price <= bids_.best_->price_at_limit_)
  {
    bids_.market(
        order,
        [&](UID match_uid, UID aggressor_uid, Quantity qty, Price exec_price, FillType fill_type)
        {
          SessionId resting_session_id = 0;
          auto      rest_it = uid_to_order_map_.find(match_uid);

          if (rest_it != uid_to_order_map_.end())
          {
            resting_session_id = rest_it->second->session_id_;
            if (fill_type == FillType::FULL)
            {
              Order* resting = rest_it->second;
              uid_to_order_map_.erase(rest_it);
              resting->~Order();
              pool_.deallocate(resting);
            }
          }

          if (fill_out_)
          {
            ipc::FillEvent fe(aggressor_uid, match_uid, qty, exec_price, Side::SELL, session_id,
                              symbol_id_);
            fill_out_->tryPush(fe);
            if (resting_session_id != 0)
            {
              fe.session_id_ = resting_session_id;
              fill_out_->tryPush(fe);
            }
          }
        });
    if (order->quantity_ == 0)
    {
      uid_to_order_map_.erase(order_uid);
      order->~Order();
      pool_.deallocate(order);
      return;
    }
  }

  if (tif == TimeInForce::IOC)
  {
    uid_to_order_map_.erase(order_uid);
    order->~Order();
    pool_.deallocate(order);
    return;
  }

  asks_.limit(order);
}

void LimitOrderBook::market(Side side, UID order_uid, Quantity quantity, SessionId session_id)
{
  if (side == Side::BUY)
    return marketBuy(order_uid, quantity, session_id);
  else
    return marketSell(order_uid, quantity, session_id);
}

void LimitOrderBook::marketBuy(UID order_uid, Quantity quantity, SessionId session_id)
{
  Order* order = pool_.allocate();
  new (order) Order(order_uid, 0, Side::BUY, quantity, session_id);

  asks_.market(
      order,
      [&](UID match_uid, UID aggressor_uid, Quantity qty, Price exec_price, FillType fill_type)
      {
        SessionId resting_session_id = 0;
        auto      rest_it = uid_to_order_map_.find(match_uid);

        if (rest_it != uid_to_order_map_.end())
        {
          resting_session_id = rest_it->second->session_id_;
          if (fill_type == FillType::FULL)
          {
            Order* resting = rest_it->second;
            uid_to_order_map_.erase(rest_it);
            resting->~Order();
            pool_.deallocate(resting);
          }
        }

        if (fill_out_)
        {
          ipc::FillEvent fe(aggressor_uid, match_uid, qty, exec_price, Side::BUY, session_id,
                            symbol_id_);
          fill_out_->tryPush(fe);
          if (resting_session_id != 0)
          {
            fe.session_id_ = resting_session_id;
            fill_out_->tryPush(fe);
          }
        }
      });

  order->~Order();
  pool_.deallocate(order);
}

void LimitOrderBook::marketSell(UID order_uid, Quantity quantity, SessionId session_id)
{
  Order* order = pool_.allocate();
  new (order) Order(order_uid, 0, Side::SELL, quantity, session_id);

  bids_.market(
      order,
      [&](UID match_uid, UID aggressor_uid, Quantity qty, Price exec_price, FillType fill_type)
      {
        SessionId resting_session_id = 0;
        auto      rest_it = uid_to_order_map_.find(match_uid);

        if (rest_it != uid_to_order_map_.end())
        {
          resting_session_id = rest_it->second->session_id_;
          if (fill_type == FillType::FULL)
          {
            Order* resting = rest_it->second;
            uid_to_order_map_.erase(rest_it);
            resting->~Order();
            pool_.deallocate(resting);
          }
        }

        if (fill_out_)
        {
          ipc::FillEvent fe(aggressor_uid, match_uid, qty, exec_price, Side::SELL, session_id,
                            symbol_id_);
          fill_out_->tryPush(fe);
          if (resting_session_id != 0)
          {
            fe.session_id_ = resting_session_id;
            fill_out_->tryPush(fe);
          }
        }
      });

  order->~Order();
  pool_.deallocate(order);
}

void LimitOrderBook::reduce(UID order_uid, Quantity quantity)
{
  auto it = uid_to_order_map_.find(order_uid);
  if (it == uid_to_order_map_.end())
    return;

  auto* order = it->second;
  if (quantity >= order->quantity_)
  {
    cancel(order_uid);
    return;
  }

  order->quantity_ -= quantity;
  order->parent_limit_->volume_at_limit_ -= quantity;

  if (order->side_ == Side::BUY)
    bids_.volume_of_tree_ -= quantity;
  else
    asks_.volume_of_tree_ -= quantity;
}

void LimitOrderBook::cancel(UID order_uid)
{
  auto it = uid_to_order_map_.find(order_uid);
  if (it == uid_to_order_map_.end())
    return;

  auto* order = it->second;
  if (order->side_ == Side::BUY)
    bids_.cancel(order);
  else
    asks_.cancel(order);

  uid_to_order_map_.erase(it);
  order->~Order();
  pool_.deallocate(order);
}

void LimitOrderBook::clear()
{
  for (auto& [uid, order] : uid_to_order_map_)
  {
    order->~Order();
    pool_.deallocate(order);
  }
  uid_to_order_map_.clear();
  asks_.clear();
  bids_.clear();
}
