#pragma once

#include <atomic>
#include <deque>
#include <random>
#include <string>

#include "../src/fix/FixMessageBuilder.hpp"
#include "../src/matching-engine/OrderStructures.hpp"

namespace bench
{
struct TrafficConfig
{
  std::string symbol;
  uint64_t    mid_price{100};
  uint64_t    spread{4};          // prices land in [mid-spread,mid+spread]
  double      cancel_rate{0.65};  // frac of next() calls return a cancel
  uint32_t    qty{1};
};

class TrafficGenerator
{
 public:
  explicit TrafficGenerator(TrafficConfig cfg)
      : cfg_(std::move(cfg)),
        rng_(std::random_device{}()),
        price_dist_(static_cast<double>(cfg_.mid_price), static_cast<double>(cfg_.spread)),
        cancel_dist_(0.0, 1.0)
  {
  }

  // returns fix message w/ side being the side for new lim orders
  // if cancel issued, it cancels the oldest outstanding order regardless of side
  std::string next(LOB::Side side)
  {
    if (!live_uids_.empty() && cancel_dist_(rng_) < cfg_.cancel_rate)
    {
      LOB::UID uid = live_uids_.front();
      live_uids_.pop_front();
      return fix::FixMessageBuilder::cancel(uid, cfg_.symbol);
    }
    LOB::UID uid = uid_counter_.fetch_add(1, std::memory_order_relaxed);
    uint64_t price = clampPrice(static_cast<uint64_t>(std::abs(price_dist_(rng_))));
    live_uids_.push_back(uid);
    if (side == LOB::Side::BUY)
      return fix::FixMessageBuilder::limit<LOB::Side::BUY>(uid, cfg_.qty, price, cfg_.symbol);
    return fix::FixMessageBuilder::limit<LOB::Side::SELL>(uid, cfg_.qty, price, cfg_.symbol);
  }

  std::vector<std::string> cancelAll()
  {
    std::vector<std::string> msgs;
    msgs.reserve(live_uids_.size());
    while (!live_uids_.empty())
    {
      msgs.push_back(fix::FixMessageBuilder::cancel(live_uids_.front(), cfg_.symbol));
      live_uids_.pop_front();
    }
    return msgs;
  }

  size_t liveCount() const { return live_uids_.size(); }

 private:
  uint64_t clampPrice(uint64_t p) const
  {
    if (p < 1)
      return 1;
    if (p > 16383)
      return 16383;
    return p;
  }

  TrafficConfig                          cfg_;
  std::deque<LOB::UID>                   live_uids_;
  std::mt19937                           rng_;
  std::normal_distribution<double>       price_dist_;
  std::uniform_real_distribution<double> cancel_dist_;

  static inline std::atomic<LOB::UID> uid_counter_{1'000'000};
};
}  // namespace bench