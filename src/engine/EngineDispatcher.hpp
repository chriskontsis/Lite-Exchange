#pragma once

#include <thread>
#include <unordered_map>
#include <atomic>
#include <memory>
#include <cstring>
#include "../utility/SPSC_Queue.hpp"
#include "../fix/FixMessage.hpp"
#include "../matching-engine/OrderBook.hpp"
#ifdef __APPLE__
#include <mach/thread_policy.h>
#include <mach/thread_act.h>  
#endif
#include <pthread.h>

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
  private:
    struct Engine
    {
      std::unique_ptr<SPSC_Queue<OrderRequest>> queue;
      LOB::LimitOrderBook book;
      std::thread thread;
      std::atomic<bool> running{true};

      Engine() : queue(std::make_unique<SPSC_Queue<OrderRequest>>(4096)) {}
    };

    static void matchingLoop(Engine *eng)
    {
      OrderRequest req;
      while (eng->running.load(std::memory_order_relaxed))
      {
        if (eng->queue->tryConsume(req))
        {
          switch (req.type)
          {
          case MsgType::NEW_LIMIT_ORDER:
            eng->book.limit(req.side, req.uid, req.quantity, req.price);
            break;
          case MsgType::NEW_MARKET_ORDER:
            eng->book.market(req.side, req.uid, req.quantity);
            break;
          case MsgType::CANCEL_ORDER:
            eng->book.cancel(req.uid);
            break;
          default:
            break;
          }
        }
      }
    }

    static void pinToCore(std::thread &t, int core)
    {
        #ifdef __linux__
              cpu_set_t cpuset;
              CPU_ZERO(&cpuset);
              CPU_SET(core, &cpuset);
              pthread_setaffinity_np(t.native_handle(), sizeof(cpuset), &cpuset);
        #elif defined(__APPLE__)
              thread_affinity_policy_data_t policy = {core};
              thread_policy_set(
                  pthread_mach_thread_np(t.native_handle()),
                  THREAD_AFFINITY_POLICY,
                  reinterpret_cast<thread_policy_t>(&policy),
                  THREAD_AFFINITY_POLICY_COUNT);

        #endif
    }

    std::unordered_map<uint64_t, std::unique_ptr<Engine>>::iterator
    createEngine(uint64_t key)
    {
      auto eng = std::make_unique<Engine>();
      Engine *ptr = eng.get();

      eng->thread = std::thread(matchingLoop, ptr);
      pinToCore(eng->thread, nextCore_++);
      auto [it, _] = engines.emplace(key, std::move(eng));
      return it;
    }

    std::unordered_map<uint64_t, std::unique_ptr<Engine>> engines;
    int nextCore_ = 0;

  public:
    ~EngineDispatcher()
    {
      for (auto &[symbol, eng] : engines)
      {
        eng->running.store(false, std::memory_order_relaxed);
        eng->thread.join();
      }
    }

    void route(const OrderRequest &req)
    {
      uint64_t key = symbolToKey(req.symbol);
      auto it = engines.find(key);

      if (it == engines.end())
      {
        it = createEngine(key);
      }
      it->second->queue->push(req);
    }
  };

}; // namespace fix