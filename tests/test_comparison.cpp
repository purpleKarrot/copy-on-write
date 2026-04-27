#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

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

TEST_CASE("operator== returns true for equal values")
{
    xyz::copy_on_write<int> a(5), b(5);
    CHECK(a == b);
}

TEST_CASE("operator== returns false for different values")
{
    xyz::copy_on_write<int> a(5), b(6);
    CHECK_FALSE(a == b);
}

TEST_CASE("operator== short-circuits via identical_to when sharing")
{
    xyz::copy_on_write<int> a(5);
    xyz::copy_on_write<int> b(a);
    REQUIRE(a.identical_to(b));
    CHECK(a == b);
}

TEST_CASE("operator== both valueless are equal")
{
    xyz::copy_on_write<int> a(1);
    xyz::copy_on_write<int> b(std::move(a)); // a is valueless
    xyz::copy_on_write<int> c(2);
    xyz::copy_on_write<int> d(std::move(c)); // c is valueless
    CHECK(a == c); // both valueless
}

TEST_CASE("operator== one valueless, one live is not equal")
{
    xyz::copy_on_write<int> a(1);
    xyz::copy_on_write<int> b(std::move(a));
    xyz::copy_on_write<int> c(1);
    CHECK_FALSE(a == c);
    CHECK_FALSE(c == a);
}

// ---------------------------------------------------------------------------
// operator== between copy_on_write and raw value
// ---------------------------------------------------------------------------

TEST_CASE("operator== with raw value returns true when equal")
{
    xyz::copy_on_write<int> x(7);
    CHECK(x == 7);
}

TEST_CASE("operator== with raw value returns false when not equal")
{
    xyz::copy_on_write<int> x(7);
    CHECK_FALSE(x == 8);
}

TEST_CASE("operator== valueless with raw value returns false")
{
    xyz::copy_on_write<int> a(1);
    xyz::copy_on_write<int> b(std::move(a));
    CHECK_FALSE(a == 1);
}

// ---------------------------------------------------------------------------
// operator<=> between two copy_on_write
// ---------------------------------------------------------------------------

TEST_CASE("operator<=> equal values yields equivalent")
{
    xyz::copy_on_write<int> a(3), b(3);
    CHECK((a <=> b) == 0);
}

TEST_CASE("operator<=> less yields less")
{
    xyz::copy_on_write<int> a(2), b(3);
    CHECK((a <=> b) < 0);
}

TEST_CASE("operator<=> greater yields greater")
{
    xyz::copy_on_write<int> a(4), b(3);
    CHECK((a <=> b) > 0);
}

TEST_CASE("operator<=> both valueless yields equal")
{
    xyz::copy_on_write<int> a(0);
    xyz::copy_on_write<int> b(std::move(a));
    xyz::copy_on_write<int> c(0);
    xyz::copy_on_write<int> d(std::move(c));
    CHECK((a <=> c) == 0);
}

TEST_CASE("operator<=> valueless is less than live")
{
    xyz::copy_on_write<int> a(0);
    xyz::copy_on_write<int> b(std::move(a)); // a is now valueless
    xyz::copy_on_write<int> c(0);
    CHECK((a <=> c) < 0);
    CHECK((c <=> a) > 0);
}

TEST_CASE("operator<=> short-circuits via identical_to to equal when sharing")
{
    xyz::copy_on_write<int> a(5);
    xyz::copy_on_write<int> b(a);
    REQUIRE(a.identical_to(b));
    CHECK((a <=> b) == 0);
}

// ---------------------------------------------------------------------------
// operator<=> between copy_on_write and raw value
// ---------------------------------------------------------------------------

TEST_CASE("operator<=> with raw value: equal")
{
    xyz::copy_on_write<int> x(3);
    CHECK((x <=> 3) == 0);
}

TEST_CASE("operator<=> with raw value: less")
{
    xyz::copy_on_write<int> x(2);
    CHECK((x <=> 3) < 0);
}

TEST_CASE("operator<=> with raw value: greater")
{
    xyz::copy_on_write<int> x(4);
    CHECK((x <=> 3) > 0);
}

TEST_CASE("operator<=> valueless with raw value yields less")
{
    xyz::copy_on_write<int> a(0);
    xyz::copy_on_write<int> b(std::move(a));
    CHECK((a <=> 0) < 0);
}

// ---------------------------------------------------------------------------
// synth_three_way fallback for types with only operator<
// ---------------------------------------------------------------------------

TEST_CASE("comparison works for types that only provide operator<")
{
    xyz::copy_on_write<LessOnly> a(LessOnly{1});
    xyz::copy_on_write<LessOnly> b(LessOnly{2});
    xyz::copy_on_write<LessOnly> c(LessOnly{1});

    CHECK((a <=> b) < 0);
    CHECK((b <=> a) > 0);
    CHECK((a <=> c) == 0);
}
