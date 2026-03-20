#pragma once
#include <cstring>
#include "../fix/FixMessage.hpp"

namespace ipc {
    struct OrderEvent {
        fix::MsgType type_ {fix::MsgType::UNKNOWN};
        LOB::Side side_ {LOB::Side::BUY};
        LOB::UID uid_ {0};
        LOB::Quantity quantity_ {0};
        LOB::Price price_ {0};
        char symbol_[8] = {};
        LOB::SessionId session_id_ {0};

        OrderEvent() = default;
        OrderEvent(const fix::OrderRequest& req, LOB::SessionId sid)
        : type_(req.type), side_(req.side), uid_(req.uid), quantity_(req.quantity), price_(req.price), session_id_(sid)
        {
            std::memcpy(symbol_, req.symbol, 8);
        }
    };

} // namespace ipc