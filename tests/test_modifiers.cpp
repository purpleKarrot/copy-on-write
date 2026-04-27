#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <copy_on_write.hpp>

#include <string>

// ---------------------------------------------------------------------------
// modify(action) — single-argument overload
// ---------------------------------------------------------------------------

TEST_CASE("modify(action) mutates value in-place when use_count == 1")
{
    xyz::copy_on_write<int> x(5);
    const int* ptr_before = &(*x);
    x.modify([](int& v) { v += 10; });
    CHECK(*x == 15);
    // No new allocation: the pointer to the value should be unchanged.
    CHECK(&(*x) == ptr_before);
}

TEST_CASE("modify(action) deep-copies before mutating when use_count > 1")
{
    xyz::copy_on_write<int> a(5);
    xyz::copy_on_write<int> b(a);
    REQUIRE(a.identical_to(b));

    b.modify([](int& v) { v += 10; });

    CHECK(*b == 15);
    CHECK(*a == 5);          // original unaffected
    CHECK_FALSE(a.identical_to(b));
}

TEST_CASE("modify(action) on shared object leaves original use_count at 1")
{
    xyz::copy_on_write<int> a(1);
    xyz::copy_on_write<int> b(a);
    b.modify([](int& v) { v = 99; });
    CHECK(a.use_count() == 1);
    CHECK(b.use_count() == 1);
}

// ---------------------------------------------------------------------------
// modify(action, transform) — two-argument overload
// ---------------------------------------------------------------------------

TEST_CASE("modify(action,transform) calls action in-place when unshared")
{
    xyz::copy_on_write<std::string> x("hello");
    bool action_called = false;
    bool transform_called = false;

    x.modify(
        [&](std::string& s) { s += "!"; action_called = true; },
        [&](std::string const& s) -> std::string { transform_called = true; return s + "!"; }
    );

    CHECK(*x == "hello!");
    CHECK(action_called);
    CHECK_FALSE(transform_called);
}

TEST_CASE("modify(action,transform) calls transform when shared")
{
    xyz::copy_on_write<std::string> a("hello");
    xyz::copy_on_write<std::string> b(a);
    bool action_called = false;
    bool transform_called = false;

    b.modify(
        [&](std::string& s) { s += "!"; action_called = true; },
        [&](std::string const& s) -> std::string { transform_called = true; return s + "?"; }
    );

    CHECK(*b == "hello?");
    CHECK(*a == "hello");      // original unaffected
    CHECK(transform_called);
    CHECK_FALSE(action_called);
}

// ---------------------------------------------------------------------------
// modify(transform) — transformation-only overload
// ---------------------------------------------------------------------------

TEST_CASE("modify(transform) produces the correct result when unshared")
{
    xyz::copy_on_write<int> x(3);
    x.modify([](int const& v) -> int { return v * 2; });
    CHECK(*x == 6);
}

TEST_CASE("modify(transform) produces the correct result when shared")
{
    xyz::copy_on_write<int> a(3);
    xyz::copy_on_write<int> b(a);
    b.modify([](int const& v) -> int { return v * 2; });
    CHECK(*b == 6);
    CHECK(*a == 3);
}

TEST_CASE("modify(transform) breaks sharing when use_count > 1")
{
    xyz::copy_on_write<int> a(3);
    xyz::copy_on_write<int> b(a);
    REQUIRE(a.identical_to(b));
    b.modify([](int const& v) -> int { return v * 2; });
    CHECK_FALSE(a.identical_to(b));
}

// ---------------------------------------------------------------------------
// swap (member)
// ---------------------------------------------------------------------------

TEST_CASE("member swap exchanges values")
{
    xyz::copy_on_write<int> a(1);
    xyz::copy_on_write<int> b(2);
    a.swap(b);
    CHECK(*a == 2);
    CHECK(*b == 1);
}

TEST_CASE("member swap with valueless object")
{
    xyz::copy_on_write<int> a(42);
    xyz::copy_on_write<int> b(std::move(a));
    // a is now valueless, b holds 42
    b.swap(a);
    CHECK(*a == 42);
    CHECK(b.valueless_after_move());
}

// ---------------------------------------------------------------------------
// swap (free function)
// ---------------------------------------------------------------------------

TEST_CASE("free swap delegates to member swap")
{
    xyz::copy_on_write<int> a(10);
    xyz::copy_on_write<int> b(20);
    using std::swap;
    swap(a, b);
    CHECK(*a == 20);
    CHECK(*b == 10);
}
