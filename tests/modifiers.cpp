// SPDX-License-Identifier: BSL-1.0

#include <copy_on_write.hpp>
#include <gtest/gtest.h>

#include <string>

// ---------------------------------------------------------------------------
// modify(action) — single-argument overload
// ---------------------------------------------------------------------------

TEST(Modifiers, ModifyActionMutatesInPlaceWhenUnshared)
{
  xyz::copy_on_write<int> x(5);
  int const* ptr_before = &(*x);
  x.modify([](int& v) { v += 10; });
  EXPECT_EQ(*x, 15);
  EXPECT_EQ(&(*x), ptr_before); // No new allocation: the pointer to the value should be unchanged.
}

TEST(Modifiers, ModifyActionDeepCopiesBeforeMutatingWhenShared)
{
  xyz::copy_on_write<int> a(5);
  xyz::copy_on_write<int> b(a);
  ASSERT_TRUE(a.identical_to(b));

  b.modify([](int& v) { v += 10; });

  EXPECT_EQ(*b, 15);
  EXPECT_EQ(*a, 5); // original unaffected
  EXPECT_FALSE(a.identical_to(b));
}

TEST(Modifiers, ModifyActionOnSharedLeavesOriginalUseCountAtOne)
{
  xyz::copy_on_write<int> a(1);
  xyz::copy_on_write<int> b(a);
  b.modify([](int& v) { v = 99; });
  EXPECT_EQ(a.use_count(), 1);
  EXPECT_EQ(b.use_count(), 1);
}

// ---------------------------------------------------------------------------
// modify(action, transform) — two-argument overload
// ---------------------------------------------------------------------------

TEST(Modifiers, ModifyActionTransformCallsActionInPlaceWhenUnshared)
{
  xyz::copy_on_write<std::string> x("hello");
  bool action_called = false;
  bool transform_called = false;

  x.modify(
    [&](std::string& s) {
      action_called = true;
      s += "!";
    },
    [&](std::string const& s) -> std::string {
      transform_called = true;
      return s + "!";
    });

  EXPECT_EQ(*x, "hello!");
  EXPECT_TRUE(action_called);
  EXPECT_FALSE(transform_called);
}

TEST(Modifiers, ModifyActionTransformCallsTransformWhenShared)
{
  xyz::copy_on_write<std::string> a("hello");
  xyz::copy_on_write<std::string> b(a);
  bool action_called = false;
  bool transform_called = false;

  b.modify(
    [&](std::string& s) {
      action_called = true;
      s += "!";
    },
    [&](std::string const& s) -> std::string {
      transform_called = true;
      return s + "?";
    });

  EXPECT_EQ(*b, "hello?");
  EXPECT_EQ(*a, "hello"); // original unaffected
  EXPECT_TRUE(transform_called);
  EXPECT_FALSE(action_called);
}

// ---------------------------------------------------------------------------
// modify(transform) — transformation-only overload
// ---------------------------------------------------------------------------

TEST(Modifiers, ModifyTransformProducesCorrectResultWhenUnshared)
{
  xyz::copy_on_write<int> x(3);
  x.modify([](int const& v) -> int { return v * 2; });
  EXPECT_EQ(*x, 6);
}

TEST(Modifiers, ModifyTransformProducesCorrectResultWhenShared)
{
  xyz::copy_on_write<int> a(3);
  xyz::copy_on_write<int> b(a);
  b.modify([](int const& v) -> int { return v * 2; });
  EXPECT_EQ(*b, 6);
  EXPECT_EQ(*a, 3);
}

TEST(Modifiers, ModifyTransformBreaksSharingWhenShared)
{
  xyz::copy_on_write<int> a(3);
  xyz::copy_on_write<int> b(a);
  ASSERT_TRUE(a.identical_to(b));
  b.modify([](int const& v) -> int { return v * 2; });
  EXPECT_FALSE(a.identical_to(b));
}

// ---------------------------------------------------------------------------
// swap (member)
// ---------------------------------------------------------------------------

TEST(Modifiers, MemberSwapExchangesValues)
{
  xyz::copy_on_write<int> a(1);
  xyz::copy_on_write<int> b(2);
  a.swap(b);
  EXPECT_EQ(*a, 2);
  EXPECT_EQ(*b, 1);
}

TEST(Modifiers, MemberSwapWithValuelessObject)
{
  xyz::copy_on_write<int> a(42);
  xyz::copy_on_write<int> b(std::move(a));
  // a is now valueless, b holds 42
  b.swap(a);
  EXPECT_EQ(*a, 42);
  EXPECT_TRUE(b.valueless_after_move());
}

// ---------------------------------------------------------------------------
// swap (free function)
// ---------------------------------------------------------------------------

TEST(Modifiers, FreeSwapDelegatesToMemberSwap)
{
  xyz::copy_on_write<int> a(10);
  xyz::copy_on_write<int> b(20);
  using std::swap;
  swap(a, b);
  EXPECT_EQ(*a, 20);
  EXPECT_EQ(*b, 10);
}
