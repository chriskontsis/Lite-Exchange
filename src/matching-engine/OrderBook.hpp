#ifndef LIMIT_ORDER_BOOK_HPP
#define LIMIT_ORDER_BOOK_HPP


#include <unordered_map>
#include <memory>
#include <functional>
#include "OrderStructures.hpp"
#include "LimitTree.hpp"
#include "../fix/FillReport.hpp"

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
            void limitSell(UID orderUID, Quantity quantity, Price price, const std::function<void(fix::FillReport)>& onFill = {});
            void limitBuy(UID orderUID, Quantity quantity, Price price, const std::function<void(fix::FillReport)>& onFill = {});
            void limit(Side side, UID orderUID, Quantity quantity, Price price, const std::function<void(fix::FillReport)>& onFill = {});
            void market(Side side, UID orderUID, Quantity quantity, const std::function<void(fix::FillReport)>& onFill = {});
            void marketBuy(UID orderUID, Quantity quantity, const std::function<void(fix::FillReport)>& onFill = {});
            void marketSell(UID orderUID, Quantity quantity, const std::function<void(fix::FillReport)>& onFill = {});
            void reduce(UID orderUID, Quantity quantity);
            void cancel(UID orderUId);
            Price bestBid() const { return bids.best ? bids.best->priceAtLimit : 0; }
            Price bestAsk() const { return asks.best ? asks.best->priceAtLimit : 0; }
            Quantity volumeAt(Side side, Price price) const {
                return (side == Side::BUY) ? bids.volumeAt(price) : asks.volumeAt(price);
            }
            bool hasOrder(UID uid) const { return UIDtoOrderMap.contains(uid); } 
    };
};


#endif