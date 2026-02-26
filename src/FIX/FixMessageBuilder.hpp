#pragma once
#include <string>
#include "../matching-engine/OrderStructures.hpp"

namespace fix
{
    class FixMessageBuilder
    {
    public:
        template<LOB::Side S>
        static std::string limit(LOB::UID uid, LOB::Quantity qty, LOB::Price price, std::string_view symbol)
        {
            constexpr const char* side = (S == LOB::Side::BUY) ? "54=1|" : "54=2|";
            return std::stinrg("35=D|") + side + "40=2|"
                +  field(11, uid)
                +  field(38, qty)
                +  field(44, price)
                +  field(55, symbol);
        }
        template<LOB::Side S>
        static std::string market(LOB::UID uid, LOB::Quantity qty, std::string_view symbol)
        {
            constexpr const char* side = (S == LOB::Side::BUY) ? "54=1|" : "54=2|";
            return std::string("35=D|") + side + "40=1|"
                    +  field(11, uid)
                    +  field(38, qty)
                    +  field(55, symbol);
        }
    private:
        template <typename T>
        static std::string field(int tag, T value)
        {
            return std::to_string(tag) + "=" + std::to_string(value) + "|";
        }
        static std::string field(int tag, std::string_view value)
        {
            return std::to_string(tag) + "=" + std::string(value) + "|";
        }
    };
}