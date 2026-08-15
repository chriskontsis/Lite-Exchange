#pragma once
#include <array>
#include <cstdint>

#include "lx/engine/shard_router.hpp"

namespace lx::engine
{
template <typename Shard, uint32_t N>
class ShardSet
{
 public:
  static constexpr uint32_t SHARDS = N;

  explicit ShardSet(std::array<Shard*, N> shards) : shards_(shards), router_(shards) {}

  ShardRouter<Shard, N>& router() { return router_; }

  auto& outbound(uint32_t shard_id) { return shards_[shard_id]->outbound(); }

 private:
  std::array<Shard*, N> shards_;
  ShardRouter<Shard, N> router_;
};
}  // namespace lx::engine