#pragma once
#include "../matching-engine/OrderStructures.hpp"

namespace fix
{
struct FillReport
{
  LOB::UID      aggressor_uid_ = 0;
  LOB::UID      resting_uid_ = 0;
  LOB::Quantity filled_qty_ = 0;
  LOB::Price    exec_price_ = 0;
  LOB::Side     side_ = LOB::Side::BUY;
  char          symbol_[8] = {};

  FillReport() = default;
  FillReport(LOB::UID aggressor, LOB::UID resting, LOB::Quantity qty, LOB::Price price, LOB::Side s)
      : aggressor_uid_(aggressor),
        resting_uid_(resting),
        filled_qty_(qty),
        exec_price_(price),
        side_(s)
  {
  }
};
}  // namespace fix
