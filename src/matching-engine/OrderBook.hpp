#ifndef LIMIT_ORDER_BOOK_HPP
#define LIMIT_ORDER_BOOK_HPP


#include <unordered_map>
#include <memory>
#include "OrderStructures.hpp"
#include "LimitTree.hpp"

namespace LOB
{
    using UIDOrderMap = std::unordered_map<UID, std::shared_ptr<Order>>;
    class LimitOrderBook 
    {
        private:
            LimitTree<Side::SELL> asks;
            LimitTree<Side::BUY> bids;
            UIDOrderMap UIDtoOrderMap;

        public:
            LimitOrderBook() : asks(), bids(), UIDtoOrderMap() {}

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
};


#endif