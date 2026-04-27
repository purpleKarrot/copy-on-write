// SPDX-License-Identifier: BSL-1.0

#include <copy_on_write.hpp>

#include <gtest/gtest.h>

// ---------------------------------------------------------------------------
// modify(action) — single-argument overload
// ---------------------------------------------------------------------------

TEST(Modifiers, ModifyActionExclusive)
{
  xyz::copy_on_write<int> c{10};
  EXPECT_EQ(c.use_count(), 1);
  c.modify([](int& v) { v += 5; });
  EXPECT_EQ(*c, 15);
  EXPECT_EQ(c.use_count(), 1);
}

TEST(Modifiers, ModifyActionDetachesWhenShared)
{
  xyz::copy_on_write<int> a{10};
  xyz::copy_on_write<int> b{a};
  EXPECT_TRUE(a.identical_to(b));

  b.modify([](int& v) { v = 20; });

  EXPECT_FALSE(a.identical_to(b));
  EXPECT_EQ(*a, 10);
  EXPECT_EQ(*b, 20);
  EXPECT_EQ(a.use_count(), 1);
  EXPECT_EQ(b.use_count(), 1);
}

TEST(Modifiers, ModifyActionString)
{
  xyz::copy_on_write<std::string> a{"foo"};
  xyz::copy_on_write<std::string> b{a};

  b.modify([](std::string& s) { s += "bar"; });

  EXPECT_EQ(*a, "foo");
  EXPECT_EQ(*b, "foobar");
}

// ---------------------------------------------------------------------------
// modify(action, transform) — two-argument overload
// ---------------------------------------------------------------------------

TEST(Modifiers, ModifyActionTransformUsesActionWhenExclusive)
{
  bool action_called = false;
  bool transform_called = false;

  xyz::copy_on_write<int> c{1};
  c.modify(
    [&](int& v) {
      action_called = true;
      v = 42;
    },
    [&](int const& v) -> int {
      transform_called = true;
      return v * 2;
    });

  EXPECT_TRUE(action_called);
  EXPECT_FALSE(transform_called);
  EXPECT_EQ(*c, 42);
}

TEST(Modifiers, ModifyActionTransformUsesTransformWhenShared)
{
  bool action_called = false;
  bool transform_called = false;

  xyz::copy_on_write<int> a{5};
  xyz::copy_on_write<int> b{a};

  b.modify(
    [&](int& v) {
      action_called = true;
      v = 0;
    },
    [&](int const& v) -> int {
      transform_called = true;
      return v * 2;
    });

  EXPECT_FALSE(action_called);
  EXPECT_TRUE(transform_called);
  EXPECT_EQ(*a, 5);
  EXPECT_EQ(*b, 10);
}

// ---------------------------------------------------------------------------
// modify(transform) — transform-only overload
// ---------------------------------------------------------------------------

TEST(Modifiers, ModifyTransformExclusive)
{
  xyz::copy_on_write<int> c{3};
  c.modify([](int const& v) -> int { return v * v; });
  EXPECT_EQ(*c, 9);
  EXPECT_EQ(c.use_count(), 1);
}

TEST(Modifiers, ModifyTransformDetachesWhenShared)
{
  xyz::copy_on_write<int> a{4};
  xyz::copy_on_write<int> b{a};

  b.modify([](int const& v) -> int { return v + 100; });

  EXPECT_EQ(*a, 4);
  EXPECT_EQ(*b, 104);
  EXPECT_FALSE(a.identical_to(b));
}

// ---------------------------------------------------------------------------
// swap
// ---------------------------------------------------------------------------

TEST(Modifiers, SwapValues)
{
  xyz::copy_on_write<int> a{1};
  xyz::copy_on_write<int> b{2};
  a.swap(b);
  EXPECT_EQ(*a, 2);
  EXPECT_EQ(*b, 1);
}

TEST(Modifiers, FreeSwap)
{
  xyz::copy_on_write<int> a{10};
  xyz::copy_on_write<int> b{20};
  using std::swap;
  swap(a, b);
  EXPECT_EQ(*a, 20);
  EXPECT_EQ(*b, 10);
}

TEST(Modifiers, SwapPreservesSharing)
{
  xyz::copy_on_write<int> a{7};
  xyz::copy_on_write<int> b{a};
  xyz::copy_on_write<int> c{99};
  EXPECT_TRUE(a.identical_to(b));

  a.swap(c);

  EXPECT_EQ(*a, 99);
  EXPECT_EQ(*b, 7);
  EXPECT_EQ(*c, 7);
  EXPECT_TRUE(b.identical_to(c));
}
