#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <thread>

#include "../ipc/FillEvent.hpp"
#include "../ipc/MPSC_Queue.hpp"
#include "../ipc/OrderEvent.hpp"
#include "../matching-engine/OrderBook.hpp"
#include "utility/ThreadAffinity.hpp"

namespace fix
{
class EngineDispatcher
{
 public:
  EngineDispatcher(MPSC_Queue<ipc::OrderEvent, 65536>& input,
                   MPSC_Queue<ipc::FillEvent, 65536>&  output)
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
  MPSC_Queue<ipc::OrderEvent, 65536>&                     input_;
  MPSC_Queue<ipc::FillEvent, 65536>&                      output_;
  std::array<std::unique_ptr<LOB::LimitOrderBook>, 65536> books_;
  std::thread                                             match_thread_;
  std::atomic<bool>                                       running_{true};

  LOB::LimitOrderBook& getOrCreateBook(LOB::SymbolId id)
  {
    if (!books_[id])
      books_[id] = std::make_unique<LOB::LimitOrderBook>(output_, id);
    return *books_[id];
  }

  void matchingLoop()
  {
    ipc::OrderEvent ev;
    util::pinToCore(0);
    while (running_.load(std::memory_order_relaxed))
    {
      if (input_.tryConsume(ev))
      {
        auto& book = getOrCreateBook(ev.symbol_id_);
        switch (ev.type_)
        {
          case fix::MsgType::NEW_LIMIT_ORDER:
            book.limit(ev.side_, ev.uid_, ev.quantity_, ev.price_, ev.session_id_,
                       ev.time_in_force_);
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
}  // namespace fix