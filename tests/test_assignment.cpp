#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <copy_on_write.hpp>

#include <string>

// ---------------------------------------------------------------------------
// Copy assignment
// ---------------------------------------------------------------------------

TEST_CASE("copy assignment sets the value")
{
    xyz::copy_on_write<int> a(1);
    xyz::copy_on_write<int> b(2);
    b = a;
    CHECK(*b == 1);
}

TEST_CASE("copy assignment shares ownership")
{
    xyz::copy_on_write<int> a(1);
    xyz::copy_on_write<int> b(2);
    b = a;
    CHECK(a.use_count() == 2);
    CHECK(b.use_count() == 2);
    CHECK(a.identical_to(b));
}

TEST_CASE("copy self-assignment is a no-op")
{
    xyz::copy_on_write<int> a(7);
    const int* ptr = &(*a);
    a = a; // NOLINT
    CHECK(*a == 7);
    CHECK(&(*a) == ptr);
    CHECK(a.use_count() == 1);
}

TEST_CASE("copy assignment releases old value when it was the sole owner")
{
    xyz::copy_on_write<int> a(99);
    xyz::copy_on_write<int> b(0);
    b = a;
    // b no longer solely owns its old model; a and b share a's model
    CHECK(a.use_count() == 2);
}

TEST_CASE("copy assignment from valueless object makes target valueless")
{
    xyz::copy_on_write<int> a(1);
    xyz::copy_on_write<int> b(std::move(a)); // a is now valueless
    xyz::copy_on_write<int> c(5);
    c = a;
    CHECK(c.valueless_after_move());
}

// ---------------------------------------------------------------------------
// Move assignment
// ---------------------------------------------------------------------------

TEST_CASE("move assignment transfers value, source becomes valueless")
{
    xyz::copy_on_write<int> a(10);
    xyz::copy_on_write<int> b(0);
    b = std::move(a);
    CHECK(a.valueless_after_move());
    CHECK(*b == 10);
}

TEST_CASE("move self-assignment is a no-op")
{
    xyz::copy_on_write<int> a(3);
    // Suppress the self-move warning; the class must handle it gracefully.
    auto& ref = a;
    a = std::move(ref);
    CHECK(*a == 3);
}

TEST_CASE("move assignment use_count stays 1 on target")
{
    xyz::copy_on_write<int> a(4);
    xyz::copy_on_write<int> b(0);
    b = std::move(a);
    CHECK(b.use_count() == 1);
}

TEST_CASE("move assignment from valueless object makes target valueless")
{
    xyz::copy_on_write<int> a(1);
    xyz::copy_on_write<int> b(std::move(a)); // a valueless
    xyz::copy_on_write<int> c(5);
    c = std::move(a);
    CHECK(c.valueless_after_move());
}

// ---------------------------------------------------------------------------
// Value assignment (operator=(U&&))
// ---------------------------------------------------------------------------

TEST_CASE("value assignment updates in-place when use_count == 1")
{
    xyz::copy_on_write<int> x(1);
    const int* ptr = &(*x);
    x = 42;
    CHECK(*x == 42);
    // With use_count == 1 the implementation mutates in place
    CHECK(&(*x) == ptr);
}

TEST_CASE("value assignment allocates a new model when shared")
{
    xyz::copy_on_write<int> a(1);
    xyz::copy_on_write<int> b(a);
    b = 42;
    CHECK(*b == 42);
    CHECK(*a == 1);
    CHECK_FALSE(a.identical_to(b));
    CHECK(a.use_count() == 1);
    CHECK(b.use_count() == 1);
}

TEST_CASE("value assignment with rvalue reference")
{
    xyz::copy_on_write<std::string> x("old");
    x = std::string("new");
    CHECK(*x == "new");
}
