// SPDX-License-Identifier: BSL-1.0
#include "incomplete_type.hpp"
#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

static_assert(std::is_nothrow_move_constructible_v<incomplete_document>);
static_assert(std::is_nothrow_move_assignable_v<incomplete_document>);

TEST(IncompleteType, OutOfLineMovesAndDestruction)
{
  EXPECT_EQ(incomplete_document::live_payloads(), 0);
  {
    incomplete_document original(42);
    auto shared = original;
    EXPECT_EQ(incomplete_document::live_payloads(), 1);
    incomplete_document moved(std::move(original));
    EXPECT_TRUE(original.empty());
    EXPECT_EQ(moved.value(), 42);
    moved.set_value(43);
    EXPECT_EQ(shared.value(), 42);
    EXPECT_EQ(incomplete_document::live_payloads(), 2);
    incomplete_document target(7);
    EXPECT_EQ(incomplete_document::live_payloads(), 3);
    target = std::move(moved);
    EXPECT_TRUE(moved.empty());
    EXPECT_EQ(target.value(), 43);
    EXPECT_EQ(incomplete_document::live_payloads(), 2);
    shared = target;
    EXPECT_EQ(incomplete_document::live_payloads(), 1);
  }
  EXPECT_EQ(incomplete_document::live_payloads(), 0);
}
