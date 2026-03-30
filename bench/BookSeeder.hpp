#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include "../src/client/FixClient.hpp"
#include "../src/fix/FixMessageBuilder.hpp"
#include "../src/matching-engine/OrderStructures.hpp"

namespace bench
{
struct SeedConfig
{
  std::string symbol_;
  uint64_t    mid_price_{100};
  int         num_levels_{20};        // price levels each side
  int         orders_per_level_{10};  // orders queued at each level
  uint32_t    qty_per_order_{1};
  int         drain_wait_ms_{100};  // sleep after sending to let engine consume
};

// seeds both sides of the book. Returns total orders sent
// bid prices : mid-1 down to nid-num_levels_ (below mid, never cross)
// ask prices: mid+1 p to mid+num_levels(above mid, never cross)
inline int seedBook(fix::FixClient& client, const SeedConfig& cfg)
{
  static std::atomic<LOB::UID> seed_uid{500'000};

  int sent = 0;
  for (int level = 1; level <= cfg.num_levels_; ++level)
  {
    uint64_t bid_price = cfg.mid_price_ - level;
    uint64_t ask_price = cfg.mid_price_ + level;

    for (int i = 0; i < cfg.orders_per_level_; ++i)
    {
      LOB::UID uid = seed_uid.fetch_add(1, std::memory_order_relaxed);
      client.send(fix::FixMessageBuilder::limit<LOB::Side::BUY>(uid, cfg.qty_per_order_, bid_price,
                                                                cfg.symbol_));
      ++sent;
      uid = seed_uid.fetch_add(1, std::memory_order_relaxed);
      client.send(fix::FixMessageBuilder::limit<LOB::Side::SELL>(uid, cfg.qty_per_order_, ask_price,
                                                                 cfg.symbol_));
      ++sent;
    }
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(cfg.drain_wait_ms_));
  return sent;
}

}  // namespace bench