#pragma once

#include <thread>
#include <unordered_map>
#include <atomic>
#include <memory>
#include <cstring>
#include "../ipc/MPSC_Queue.hpp"
#include "../ipc/OrderEvent.hpp"
#include "../ipc/FillEvent.hpp"
#include "../matching-engine/OrderBook.hpp"

namespace fix
{
  inline uint64_t symbolToKey(const char *sym)
  {
    uint64_t key = 0;
    std::memcpy(&key, sym, 8);
    return key;
  }

  class EngineDispatcher
  {
  public:
    EngineDispatcher(MPSC_Queue<ipc::OrderEvent, 4096>& input, MPSC_Queue<ipc::FillEvent, 4096>& output)
    : input_(input), output_(output)
    { 
      match_thread_ = std::thread(&EngineDispatcher::matchingLoop, this);
    }

    ~EngineDispatcher()
    {
      running_.store(false, std::memory_order_relaxed);
      match_thread_.join();
    }

  private:
    MPSC_Queue<ipc::OrderEvent, 4096> &input_;
    MPSC_Queue<ipc::FillEvent, 4096> &output_;
    std::unordered_map<uint64_t, std::unique_ptr<LOB::LimitOrderBook>> books_;
    std::thread match_thread_;
    std::atomic<bool> running_{true};

    LOB::LimitOrderBook &getOrCreateBook(uint64_t key, const char *symbol)
    {
      auto it = books_.find(key);
      if(it != books_.end())
        return *it->second;
      auto [it2, _] = 
          books_.emplace(key, std::make_unique<LOB::LimitOrderBook>(output_, symbol));
      return *it2->second;
    }

    void matchingLoop()
    {
      ipc::OrderEvent ev;
      while(running_.load(std::memory_order_relaxed))
      {
        if(input_.tryConsume(ev))
        {
          auto& book = getOrCreateBook(symbolToKey(ev.symbol_), ev.symbol_);
          switch(ev.type_)
          {
            case fix::MsgType::NEW_LIMIT_ORDER:
              book.limit(ev.side_, ev.uid_, ev.quantity_, ev.price_, ev.session_id_);
              break;
            case fix::MsgType::NEW_MARKET_ORDER:
              book.market(ev.side_, ev.uid_, ev.quantity_, ev.session_id_);
              break;
            case fix::MsgType::CANCEL_ORDER:
              book.cancel(ev.uid_);
              break;
            default:
              break;
          }
        }
      }
    }
  };
}