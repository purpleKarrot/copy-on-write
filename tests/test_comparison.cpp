#include <gtest/gtest.h>

#include <copy_on_write.hpp>

#include <compare>
#include <string>

// ---------------------------------------------------------------------------
// Helper type: only supports operator<, not operator<=>
// ---------------------------------------------------------------------------

struct LessOnly
{
    int value;
    bool operator==(LessOnly const&) const = default;
    bool operator<(LessOnly const& o) const { return value < o.value; }
};

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

TEST(Comparison, EqualityShortCircuitsViaIdenticalToWhenSharing)
{
    xyz::copy_on_write<int> a(5);
    xyz::copy_on_write<int> b(a);
    ASSERT_TRUE(a.identical_to(b));
    EXPECT_TRUE(a == b);
}

TEST(Comparison, EqualityBothValuelessAreEqual)
{
    xyz::copy_on_write<int> a(1);
    xyz::copy_on_write<int> b(std::move(a)); // a is valueless
    xyz::copy_on_write<int> c(2);
    xyz::copy_on_write<int> d(std::move(c)); // c is valueless
    EXPECT_TRUE(a == c); // both valueless
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
    EXPECT_TRUE((a <=> b) == 0);
}

TEST(Comparison, SpaceshipLessYieldsLess)
{
    xyz::copy_on_write<int> a(2), b(3);
    EXPECT_TRUE((a <=> b) < 0);
}

TEST(Comparison, SpaceshipGreaterYieldsGreater)
{
    xyz::copy_on_write<int> a(4), b(3);
    EXPECT_TRUE((a <=> b) > 0);
}

TEST(Comparison, SpaceshipBothValuelessYieldsEqual)
{
    xyz::copy_on_write<int> a(0);
    xyz::copy_on_write<int> b(std::move(a));
    xyz::copy_on_write<int> c(0);
    xyz::copy_on_write<int> d(std::move(c));
    EXPECT_TRUE((a <=> c) == 0);
}

TEST(Comparison, SpaceshipValuelessIsLessThanLive)
{
    xyz::copy_on_write<int> a(0);
    xyz::copy_on_write<int> b(std::move(a)); // a is now valueless
    xyz::copy_on_write<int> c(0);
    EXPECT_TRUE((a <=> c) < 0);
    EXPECT_TRUE((c <=> a) > 0);
}

TEST(Comparison, SpaceshipShortCircuitsViaIdenticalToWhenSharing)
{
    xyz::copy_on_write<int> a(5);
    xyz::copy_on_write<int> b(a);
    ASSERT_TRUE(a.identical_to(b));
    EXPECT_TRUE((a <=> b) == 0);
}

// ---------------------------------------------------------------------------
// operator<=> between copy_on_write and raw value
// ---------------------------------------------------------------------------

TEST(Comparison, SpaceshipWithRawValueEqual)
{
    xyz::copy_on_write<int> x(3);
    EXPECT_TRUE((x <=> 3) == 0);
}

TEST(Comparison, SpaceshipWithRawValueLess)
{
    xyz::copy_on_write<int> x(2);
    EXPECT_TRUE((x <=> 3) < 0);
}

TEST(Comparison, SpaceshipWithRawValueGreater)
{
    xyz::copy_on_write<int> x(4);
    EXPECT_TRUE((x <=> 3) > 0);
}

TEST(Comparison, SpaceshipValuelessWithRawValueYieldsLess)
{
    xyz::copy_on_write<int> a(0);
    xyz::copy_on_write<int> b(std::move(a));
    EXPECT_TRUE((a <=> 0) < 0);
}

// ---------------------------------------------------------------------------
// synth_three_way fallback for types with only operator<
// ---------------------------------------------------------------------------

TEST(Comparison, SynthThreeWayFallbackForLessOnlyTypes)
{
    xyz::copy_on_write<LessOnly> a(LessOnly{1});
    xyz::copy_on_write<LessOnly> b(LessOnly{2});
    xyz::copy_on_write<LessOnly> c(LessOnly{1});

    EXPECT_TRUE((a <=> b) < 0);
    EXPECT_TRUE((b <=> a) > 0);
    EXPECT_TRUE((a <=> c) == 0);
}

