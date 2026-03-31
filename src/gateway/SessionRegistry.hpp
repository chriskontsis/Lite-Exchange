#pragma once
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace fix
{
class FixSession;
}

namespace gateway
{
class SessionRegistry
{
  using SessionId = uint32_t;
  using SessionMap = std::unordered_map<SessionId, fix::FixSession*>;

 public:
  SessionId registerSession(fix::FixSession* s)
  {
    SessionId        id = next_id_.fetch_add(1, std::memory_order_relaxed);
    std::unique_lock<std::shared_mutex> lock(mutex_);
    sessions_[id] = s;
    return id;
  }

  void removeSession(SessionId id)
  {
    std::unique_lock<std::shared_mutex> lck(mutex_);
    sessions_.erase(id);
  }

  fix::FixSession* lookup(SessionId id) const
  {
    std::shared_lock<std::shared_mutex> lck(mutex_);
    auto             it = sessions_.find(id);
    if (it == sessions_.end())
      return nullptr;
    return it->second;
  }

 private:
  std::atomic<SessionId>    next_id_{1};
  mutable std::shared_mutex mutex_;
  SessionMap                sessions_;
};
}  // namespace gateway