#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gateway
{
class SymbolRegistry
{
 public:
  uint16_t getOrCreate(std::string_view symbol)
  {
    std::string key(symbol);
    auto        it = to_id_.find(key);
    if (it != to_id_.end())
      return it->second;

    uint16_t id = next_id_++;
    to_id_[key] = id;
    to_name_.resize(id + 1);
    to_name_[id] = key;
    return id;
  }

  std::string_view name(uint16_t id) const
  {
    if (id < to_name_.size())
      return to_name_[id];
    return {};
  }

 private:
  std::unordered_map<std::string, uint16_t> to_id_;
  std::vector<std::string>                  to_name_;
  uint16_t                                  next_id_{0};
};
}  // namespace gateway