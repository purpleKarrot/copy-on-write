// SPDX-License-Identifier: BSL-1.0

#include <copy_on_write.hpp>
#include <gtest/gtest.h>

#include <compare>
#include <limits>
#include <stdexcept>
#include <string>

// ---------------------------------------------------------------------------
// operator== between two copy_on_write
// ---------------------------------------------------------------------------

TEST(Comparison, EqualityReturnsTrueForEqualValues)
{
  xyz::copy_on_write<int> a(5), b(5);
  EXPECT_TRUE(a == b);
}

TEST(Comparison, EqualityReturnsFalseForDifferentValues)
{
  xyz::copy_on_write<int> a(5), b(6);
  EXPECT_FALSE(a == b);
}

TEST(Comparison, EqualityOfSharedIntegersReturnsTrue)
{
  xyz::copy_on_write<int> a(5);
  xyz::copy_on_write<int> b(a);
  ASSERT_TRUE(a.identical_to(b));
  EXPECT_TRUE(a == b);
}

TEST(Comparison, EqualityPreservesNaNSemanticsRegardlessOfSharing)
{
  double nan = std::numeric_limits<double>::quiet_NaN();
  xyz::copy_on_write<double> a(nan), b(a), c(nan);
  ASSERT_TRUE(a.identical_to(b));
  ASSERT_FALSE(a.identical_to(c));
  EXPECT_FALSE(a == a);
  EXPECT_FALSE(a == b);
  EXPECT_FALSE(b == a);
  EXPECT_FALSE(a == c);
  EXPECT_FALSE(a == nan);
}

TEST(Comparison, EqualityBothValuelessAreEqual)
{
  xyz::copy_on_write<int> a(1);
  xyz::copy_on_write<int> b(std::move(a)); // a is valueless
  xyz::copy_on_write<int> c(2);
  xyz::copy_on_write<int> d(std::move(c)); // c is valueless
  EXPECT_TRUE(a == c);                     // both valueless
}

TEST(Comparison, EqualityOneValuelessOneLiveIsNotEqual)
{
  xyz::copy_on_write<int> a(1);
  xyz::copy_on_write<int> b(std::move(a));
  xyz::copy_on_write<int> c(1);
  EXPECT_FALSE(a == c);
  EXPECT_FALSE(c == a);
}

// ---------------------------------------------------------------------------
// operator== between copy_on_write and raw value
// ---------------------------------------------------------------------------

TEST(Comparison, EqualityWithRawValueReturnsTrueWhenEqual)
{
  xyz::copy_on_write<int> x(7);
  EXPECT_TRUE(x == 7);
}

TEST(Comparison, EqualityWithRawValueReturnsFalseWhenNotEqual)
{
  xyz::copy_on_write<int> x(7);
  EXPECT_FALSE(x == 8);
}

TEST(Comparison, EqualityValuelessWithRawValueReturnsFalse)
{
  xyz::copy_on_write<int> a(1);
  xyz::copy_on_write<int> b(std::move(a));
  EXPECT_FALSE(a == 1);
}

// ---------------------------------------------------------------------------
// operator<=> between two copy_on_write
// ---------------------------------------------------------------------------

TEST(Comparison, SpaceshipEqualValuesYieldsEquivalent)
{
  xyz::copy_on_write<int> a(3), b(3);
  EXPECT_TRUE(std::is_eq(a <=> b));
}

TEST(Comparison, SpaceshipLessYieldsLess)
{
  xyz::copy_on_write<int> a(2), b(3);
  EXPECT_TRUE(std::is_lt(a <=> b));
}

TEST(Comparison, SpaceshipGreaterYieldsGreater)
{
  xyz::copy_on_write<int> a(4), b(3);
  EXPECT_TRUE(std::is_gt(a <=> b));
}

TEST(Comparison, SpaceshipBothValuelessYieldsEqual)
{
  xyz::copy_on_write<int> a(0);
  xyz::copy_on_write<int> b(std::move(a));
  xyz::copy_on_write<int> c(0);
  xyz::copy_on_write<int> d(std::move(c));
  EXPECT_TRUE(std::is_eq(a <=> c));
}

TEST(Comparison, SpaceshipValuelessIsLessThanLive)
{
  xyz::copy_on_write<int> a(0);
  xyz::copy_on_write<int> b(std::move(a)); // a is now valueless
  xyz::copy_on_write<int> c(0);
  EXPECT_TRUE(std::is_lt(a <=> c));
  EXPECT_TRUE(std::is_gt(c <=> a));
}

TEST(Comparison, SpaceshipOfSharedIntegersYieldsEquivalent)
{
  xyz::copy_on_write<int> a(5);
  xyz::copy_on_write<int> b(a);
  ASSERT_TRUE(a.identical_to(b));
  EXPECT_TRUE(std::is_eq(a <=> b));
}

TEST(Comparison, SpaceshipPreservesNaNSemanticsRegardlessOfSharing)
{
  double nan = std::numeric_limits<double>::quiet_NaN();
  xyz::copy_on_write<double> a(nan), b(a), c(nan);
  ASSERT_TRUE(a.identical_to(b));
  ASSERT_FALSE(a.identical_to(c));
  EXPECT_EQ(a <=> a, std::partial_ordering::unordered);
  EXPECT_EQ(a <=> b, std::partial_ordering::unordered);
  EXPECT_EQ(b <=> a, std::partial_ordering::unordered);
  EXPECT_EQ(a <=> c, std::partial_ordering::unordered);
  EXPECT_EQ(a <=> nan, std::partial_ordering::unordered);
}

TEST(Comparison, SharedCompositePreservesPayloadComparisonSemantics)
{
  struct Measurement
  {
    double value;
    bool operator==(Measurement const&) const = default;
    auto operator<=>(Measurement const&) const = default;
  };

  xyz::copy_on_write<Measurement> a(Measurement{std::numeric_limits<double>::quiet_NaN()});
  xyz::copy_on_write<Measurement> b(a);
  ASSERT_TRUE(a.identical_to(b));
  EXPECT_FALSE(a == b);
  EXPECT_EQ(a <=> b, std::partial_ordering::unordered);
}

// ---------------------------------------------------------------------------
// operator<=> between copy_on_write and raw value
// ---------------------------------------------------------------------------

TEST(Comparison, SpaceshipWithRawValueEqual)
{
  xyz::copy_on_write<int> x(3);
  EXPECT_TRUE(std::is_eq(x <=> 3));
}

TEST(Comparison, SpaceshipWithRawValueLess)
{
  xyz::copy_on_write<int> x(2);
  EXPECT_TRUE(std::is_lt(x <=> 3));
}

TEST(Comparison, SpaceshipWithRawValueGreater)
{
  xyz::copy_on_write<int> x(4);
  EXPECT_TRUE(std::is_gt(x <=> 3));
}

TEST(Comparison, SpaceshipValuelessWithRawValueYieldsLess)
{
  xyz::copy_on_write<int> a(0);
  xyz::copy_on_write<int> b(std::move(a));
  EXPECT_TRUE(std::is_lt(a <=> 0));
}

// ---------------------------------------------------------------------------
// synth_three_way fallback for types with only operator<
// ---------------------------------------------------------------------------

TEST(Comparison, SynthThreeWayFallbackForLessOnlyTypes)
{
  struct LessOnly
  {
    int value;
    bool operator==(LessOnly const&) const = default;
    bool operator<(LessOnly const& o) const { return value < o.value; }
  };

  xyz::copy_on_write<LessOnly> a(LessOnly{1});
  xyz::copy_on_write<LessOnly> b(LessOnly{2});
  xyz::copy_on_write<LessOnly> c(LessOnly{1});

  EXPECT_TRUE(std::is_lt(a <=> b));
  EXPECT_TRUE(std::is_gt(b <=> a));
  EXPECT_TRUE(std::is_eq(a <=> c));
}

namespace {
struct equality_result
{
  bool equal;
  bool throws;

  operator bool() const
  {
    if (throws) {
      throw std::runtime_error("equality conversion failed");
    }
    return equal;
  }

  friend bool operator&&(bool, equality_result)
  {
    throw std::logic_error("overloaded operator&& must not be called");
  }
};

struct proxy_equality
{
  int value;
  bool throws = false;

  equality_result operator==(proxy_equality const& other) const noexcept
  {
    return {value == other.value, throws};
  }

  equality_result operator==(int other) const noexcept
  {
    return {value == other, throws};
  }
};
} // namespace

TEST(Comparison, EqualityPropagatesThrowingBoolConversion)
{
  xyz::copy_on_write<proxy_equality> a(proxy_equality{1, true});
  auto b = a;
  static_assert(!noexcept(a == b));
  static_assert(!noexcept(a == 1));
  EXPECT_THROW((void)(a == b), std::runtime_error);
  EXPECT_THROW((void)(a == 1), std::runtime_error);
}

TEST(Comparison, RawEqualityDoesNotInvokeOverloadedLogicalAnd)
{
  xyz::copy_on_write<proxy_equality> a(proxy_equality{1});
  EXPECT_TRUE(a == 1);
  EXPECT_FALSE(a == 2);
}

TEST(Comparison, ValuelessEqualityDoesNotEvaluatePayloadComparison)
{
  xyz::copy_on_write<proxy_equality> a(proxy_equality{1, true});
  auto live = std::move(a);
  EXPECT_FALSE(a == live);
  EXPECT_FALSE(live == a);
  EXPECT_FALSE(a == 1);
  EXPECT_TRUE(a == a);
}
