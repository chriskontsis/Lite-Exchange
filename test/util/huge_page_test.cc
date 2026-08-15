#include "lx/util/huge_page.hpp"

#include <gtest/gtest.h>

TEST(HugePage, WriteAndReadBack)
{
  lx::util::HugePage<uint64_t, 512> page;
  for (uint32_t i = 0; i < 512; ++i)
    page[i] = static_cast<uint64_t>(i) * 2;
  for (uint32_t i = 0; i < 512; ++i)
    EXPECT_EQ(page[i], static_cast<uint64_t>(i) * 2);
}

TEST(HugePage, GetReturnsValidPointer)
{
  lx::util::HugePage<int, 256> page;
  page[0] = 99;
  EXPECT_EQ(page.get()[0], 99);
}