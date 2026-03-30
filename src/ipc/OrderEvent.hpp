#pragma once

#include "../fix/FixMessage.hpp"

namespace ipc
{
struct OrderEvent
{
  fix::MsgType     type_{fix::MsgType::UNKNOWN};
  LOB::Side        side_{LOB::Side::BUY};
  LOB::UID         uid_{0};
  LOB::Quantity    quantity_{0};
  LOB::Price       price_{0};
  LOB::SymbolId    symbol_id_{0};
  LOB::SessionId   session_id_{0};
  LOB::TimeInForce time_in_force_{LOB::TimeInForce::GTC};

  OrderEvent() = default;
  OrderEvent(const fix::OrderRequest& req, LOB::SessionId sid)
      : type_(req.type),
        side_(req.side),
        uid_(req.uid),
        quantity_(req.quantity),
        price_(req.price),
        symbol_id_(req.symbol_id),
        session_id_(sid),
        time_in_force_(req.time_in_force)
  {
  }
};

}  // namespace ipc