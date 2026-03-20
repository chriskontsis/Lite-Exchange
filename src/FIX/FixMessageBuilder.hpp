#pragma once
#include <cstring>
#include <string>
#include "../ipc/FillEvent.hpp"
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
            return std::string("35=D|") + side + "40=2|"
                +  field(11, uid)
                +  field(38, qty)
                +  field(44, price)
                +  field(55, symbol) + "\n";
        }
        template<LOB::Side S>
        static std::string market(LOB::UID uid, LOB::Quantity qty, std::string_view symbol)
        {
            constexpr const char* side = (S == LOB::Side::BUY) ? "54=1|" : "54=2|";
            return std::string("35=D|") + side + "40=1|"
                    +  field(11, uid)
                    +  field(38, qty)
                    +  field(55, symbol) + "\n";
        }

        static std::string cancel(LOB::UID uid, std::string_view symbol)
        {
            return std::string("35=F|")
                +  field(11,uid)
                +  field(55, symbol) + "\n";
        }

        static std::string executionReport(const ipc::FillEvent& fe)
        {
            const char* side = (fe.side == LOB::Side::BUY) ? "54=1|" : "54=2|";
            char sym[9] = {};
            std::memcpy(sym, fe.symbol_, 8);
            return std::string("35=8|") + side
                    +  field(11, fe.aggressor_uid_)
                    +  field(31, fe.exec_price_)
                    +  field(32, fe.filled_qty_) 
                    +  field(55, std::string_view(sym, std::strlen(sym))) + "\n";
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