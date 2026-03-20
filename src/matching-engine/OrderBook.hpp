#ifndef LIMIT_ORDER_BOOK_HPP
#define LIMIT_ORDER_BOOK_HPP


#include <unordered_map>
#include <memory>
#include "OrderStructures.hpp"
#include "LimitTree.hpp"
#include "../ipc/FillEvent.hpp"
#include "../ipc/MPSC_Queue.hpp"

namespace LOB
{
    using UIDOrderMap = std::unordered_map<UID, std::shared_ptr<Order>>;
    class LimitOrderBook 
    {
        private:
            LimitTree<Side::SELL> asks;
            LimitTree<Side::BUY> bids;
            UIDOrderMap UIDtoOrderMap;
            MPSC_Queue<ipc::FillEvent, 4096>* fillOut_ {nullptr};
            char symbol_[8] = {};

            void limitSell(UID orderUID, Quantity quantity, Price price, SessionId session_id);
            void limitBuy(UID orderUID, Quantity quantity, Price price, SessionId session_id);
            void marketBuy(UID orderUID, Quantity quantity, SessionId session_id);
            void marketSell(UID orderUID, Quantity quantity, SessionId session_id);

        public:
            LimitOrderBook() = default;
            LimitOrderBook(MPSC_Queue<ipc::FillEvent, 4096>& fillOut, const char* symbol) 
                : fillOut_(&fillOut) 
            {
                std::memcpy(symbol_, symbol, 8);
            }

            void clear();
            void limit(Side side, UID orderUID, Quantity quantity, Price price, SessionId session_id = 0);
            void market(Side side, UID orderUID, Quantity quantity, SessionId session_id = 0);
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