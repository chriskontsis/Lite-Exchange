#ifndef FIX_MESSAGE_HPP
#define FIX_MESSAGE_HPP

#include <string>
#include <vector>
#include "../matching-engine/OrderStructures.hpp"

namespace fix {

// What kind of action this message requests.
enum class MsgType {
    NEW_LIMIT_ORDER,
    NEW_MARKET_ORDER,
    CANCEL_ORDER,
    UNKNOWN
};

// The payload that travels through the SPSC queue from the I/O thread
// to the matching engine thread. Contains everything the engine needs
// to process one order action.
struct OrderRequest {
    MsgType      type     = MsgType::UNKNOWN;
    LOB::Side    side     = LOB::Side::BUY;
    LOB::UID     uid      = 0;
    LOB::Quantity quantity = 0;
    LOB::Price   price    = 0;  // 0 for market orders
};

class FixMessage {

};

} // namespace fix

#endif