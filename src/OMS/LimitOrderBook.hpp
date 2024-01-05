#ifndef LIMIT_ORDER_BOOK_HPP
#define LIMIT_ORDER_BOOK_HPP

#include "LimitTree.hpp"
#include <tuple>
#include <string>

namespace LOB
{

    using UIDOrderMap = std::unordered_map<UID, Order>;

    class LimitOrderBook
    {
    private:
        LimitTree<Side::SELL> asks;
        LimitTree<Side::BUY> bids;
        UIDOrderMap uidOrderMap;

    public:
        LimitOrderBook() : asks(), bids(), uidOrderMap() {}
        void clear();
        void limitSell(UID orderUID, Quantity quantity, Price price);
        void limitBuy(UID orderUID, Quantity quantity, Price price);
        void limit(Side side, UID orderUID, Quantity quantity, Price price);
        void market(Side side, UID orderUID, Quantity quantity);
        void marketBuy(UID orderUID, Quantity quantity);
        void marketSell(UID orderUID, Quantity quantity);
        void reduce(UID orderUID, Quantity quantity);
        void cancel(UID orderUId);
    };

}
#endif