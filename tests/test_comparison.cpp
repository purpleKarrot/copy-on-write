// SPDX-License-Identifier: BSL-1.0

#include <copy_on_write.hpp>

#include <gtest/gtest.h>

// ---------------------------------------------------------------------------
// operator== between two copy_on_write objects
// ---------------------------------------------------------------------------

TEST(Comparison, EqualSameValue)
{
  xyz::copy_on_write<int> a{5};
  xyz::copy_on_write<int> b{5};
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a != b);
}

TEST(Comparison, NotEqualDifferentValue)
{
  xyz::copy_on_write<int> a{1};
  xyz::copy_on_write<int> b{2};
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
}

TEST(Comparison, EqualIdenticalStorage)
{
  xyz::copy_on_write<int> a{7};
  xyz::copy_on_write<int> b{a};
  // Identical storage fast-path
  EXPECT_TRUE(a.identical_to(b));
  EXPECT_TRUE(a == b);
}

// ---------------------------------------------------------------------------
// operator== with valueless objects
// ---------------------------------------------------------------------------

TEST(Comparison, BothValuelessAreEqual)
{
  xyz::copy_on_write<int> a{1};
  xyz::copy_on_write<int> b{2};
  xyz::copy_on_write<int> va{std::move(a)};
  xyz::copy_on_write<int> vb{std::move(b)};
  EXPECT_TRUE(a == b); // both valueless
}

TEST(Comparison, ValuelessNotEqualNonValueless)
{
  xyz::copy_on_write<int> a{1};
  xyz::copy_on_write<int> b{std::move(a)};
  xyz::copy_on_write<int> c{1};
  EXPECT_FALSE(a == c);
  EXPECT_FALSE(c == a);
}

// ---------------------------------------------------------------------------
// operator== between copy_on_write and raw value
// ---------------------------------------------------------------------------

TEST(Comparison, EqualRawValue)
{
  xyz::copy_on_write<int> c{42};
  EXPECT_TRUE(c == 42);
}

TEST(Comparison, NotEqualRawValue)
{
  xyz::copy_on_write<int> c{42};
  EXPECT_FALSE(c == 0);
}

TEST(Comparison, ValuelessNotEqualRawValue)
{
  xyz::copy_on_write<int> a{1};
  xyz::copy_on_write<int> b{std::move(a)};
  EXPECT_FALSE(a == 1);
}

// ---------------------------------------------------------------------------
// operator<=> between two copy_on_write objects
// ---------------------------------------------------------------------------

TEST(Comparison, ThreeWayLess)
{
  xyz::copy_on_write<int> a{1};
  xyz::copy_on_write<int> b{2};
  EXPECT_TRUE((a <=> b) < 0);
  EXPECT_TRUE(a < b);
}

TEST(Comparison, ThreeWayGreater)
{
  xyz::copy_on_write<int> a{3};
  xyz::copy_on_write<int> b{1};
  EXPECT_TRUE((a <=> b) > 0);
  EXPECT_TRUE(a > b);
}

TEST(Comparison, ThreeWayEqual)
{
  xyz::copy_on_write<int> a{5};
  xyz::copy_on_write<int> b{5};
  EXPECT_TRUE((a <=> b) == 0);
}

TEST(Comparison, ThreeWayIdenticalStorageEqual)
{
  xyz::copy_on_write<int> a{9};
  xyz::copy_on_write<int> b{a};
  EXPECT_TRUE((a <=> b) == 0);
}

// ---------------------------------------------------------------------------
// operator<=> with valueless objects
// ---------------------------------------------------------------------------

TEST(Comparison, ValuelessLessThanNonValueless)
{
  xyz::copy_on_write<int> a{1};
  xyz::copy_on_write<int> b{std::move(a)};
  xyz::copy_on_write<int> c{1};
  // valueless is "less" than non-valueless
  EXPECT_TRUE(a < c);
  EXPECT_FALSE(c < a);
}

TEST(Comparison, BothValuelessEqual)
{
  xyz::copy_on_write<int> a{1};
  xyz::copy_on_write<int> b{2};
  xyz::copy_on_write<int> va{std::move(a)};
  xyz::copy_on_write<int> vb{std::move(b)};
  EXPECT_TRUE((a <=> b) == 0);
}

// ---------------------------------------------------------------------------
// operator<=> between copy_on_write and raw value
// ---------------------------------------------------------------------------

TEST(Comparison, ThreeWayCowVsValue)
{
  xyz::copy_on_write<int> c{5};
  EXPECT_TRUE((c <=> 5) == 0);
  EXPECT_TRUE((c <=> 10) < 0);
  EXPECT_TRUE((c <=> 1) > 0);
}

TEST(Comparison, ValuelessLessThanRawValue)
{
  xyz::copy_on_write<int> a{0};
  xyz::copy_on_write<int> b{std::move(a)};
  EXPECT_TRUE((a <=> 0) < 0);
}
