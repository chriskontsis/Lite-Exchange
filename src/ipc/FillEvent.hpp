#pragma once
#include <cstring>
#include "../matching-engine/OrderStructures.hpp"

namespace ipc
{
    struct FillEvent
    {
        LOB::UID aggressor_uid_{0};
        LOB::UID resting_uid_{0};
        LOB::Quantity filled_qty_{0};
        LOB::Price exec_price_{0};
        LOB::Side side_{LOB::Side::BUY};
        char symbol_[8] = {};
        LOB::SessionId session_id_{0};

        FillEvent() = default;
        FillEvent(LOB::UID aggressor, LOB::UID resting, LOB::Quantity qty,
                  LOB::Price price, LOB::Side s, const char *sym, LOB::SessionId sid)
            : aggressor_uid_(aggressor), resting_uid_(resting), filled_qty_(qty),
              exec_price_(price), side_(s), session_id_(sid)
        {
            std::memcpy(symbol_, sym, 8);
        }
    };
} // namespace ipc