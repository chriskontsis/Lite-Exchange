#ifndef FIX_MESSAGE_HPP
#define FIX_MESSAGE_HPP

#include <string>
#include <string_view>
#include <charconv>
#include "../matching-engine/OrderStructures.hpp"

namespace fix
{

    enum class MsgType
    {
        NEW_LIMIT_ORDER,
        NEW_MARKET_ORDER,
        CANCEL_ORDER,
        UNKNOWN
    };

    struct OrderRequest
    {
        MsgType type = MsgType::UNKNOWN;
        LOB::Side side = LOB::Side::BUY;
        LOB::UID uid = 0;
        LOB::Quantity quantity = 0;
        LOB::Price price = 0; // 0 for market orders
    };

    class FixMessage
    {
    public:
        static OrderRequest parse(std::string_view rawOrder)
        {
            OrderRequest req;

            while (!rawOrder.empty())
            {
                // find the end of current filed
                auto pipe = rawOrder.find('|');
                std::string_view field = rawOrder.substr(0, pipe);

                auto eq = field.find('=');
                if (eq != std::string_view::npos)
                {
                    std::string_view tagStr = field.substr(0, eq);
                    std::string_view val = field.substr(eq + 1);

                    int tag = 0;
                    std::from_chars(tagStr.data(), tagStr.data() + tagStr.size(), tag);

                    switch (tag)
                    {
                    case 35:
                        if (val == "D")
                            req.type = MsgType::NEW_LIMIT_ORDER;
                        else if (val == "F")
                            req.type = MsgType::CANCEL_ORDER;
                        break;
                    case 11:
                        std::from_chars(val.data(), val.data() + val.size(), req.uid);
                        break;
                    case 54:
                        req.side = (val == "1") ? LOB::Side::BUY : LOB::Side::SELL;
                        break;
                    case 38:
                        std::from_chars(val.data(), val.data() + val.size(), req.quantity);
                        break;
                    case 40:
                        if (req.type != MsgType::CANCEL_ORDER)
                            req.type = (val == "1") ? MsgType::NEW_MARKET_ORDER : MsgType::NEW_LIMIT_ORDER;
                        break;
                    case 44:
                        std::from_chars(val.data(), val.data() + val.size(), req.price);
                        break;
                    }
                }

                if(pipe == rawOrder.npos) break;
                rawOrder.remove_prefix(pipe + 1);  // advance past the '|'
            }

            return req;
        }
    };
} // namespace fix
#endif