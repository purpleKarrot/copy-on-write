// SPDX-License-Identifier: BSL-1.0

#include "composition.hpp"
#include <gtest/gtest.h>

#include <compare>

TEST(Composition, Construction)
{
  Composite c;                // default construction
  Composite d(c);             // copy construction
  Composite e = std::move(c); // move construction
  d = e;                      // copy assignment
  e = std::move(d);           // move assignment
  EXPECT_EQ(c, d);
  EXPECT_TRUE(std::is_eq(c <=> d));
}
