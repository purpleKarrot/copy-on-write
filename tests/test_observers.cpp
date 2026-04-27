#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <copy_on_write.hpp>

#include <string>

// ---------------------------------------------------------------------------
// operator*
// ---------------------------------------------------------------------------

TEST_CASE("operator* returns const reference to stored value")
{
    xyz::copy_on_write<int> x(42);
    static_assert(std::is_same_v<decltype(*x), int const&>);
    CHECK(*x == 42);
}

TEST_CASE("operator* reflects value changes via modify")
{
    xyz::copy_on_write<int> x(1);
    x.modify([](int& v) { v = 99; });
    CHECK(*x == 99);
}

// ---------------------------------------------------------------------------
// operator->
// ---------------------------------------------------------------------------

TEST_CASE("operator-> provides const access to member")
{
    xyz::copy_on_write<std::string> x("hello");
    CHECK(x->size() == 5u);
    CHECK(x->front() == 'h');
}

// ---------------------------------------------------------------------------
// valueless_after_move
// ---------------------------------------------------------------------------

TEST_CASE("valueless_after_move is false for a live object")
{
    xyz::copy_on_write<int> x(0);
    CHECK_FALSE(x.valueless_after_move());
}

TEST_CASE("valueless_after_move is true after move construction")
{
    xyz::copy_on_write<int> a(5);
    auto b = std::move(a);
    CHECK(a.valueless_after_move());
    CHECK_FALSE(b.valueless_after_move());
}

TEST_CASE("valueless_after_move is true after move assignment")
{
    xyz::copy_on_write<int> a(5);
    xyz::copy_on_write<int> b(0);
    b = std::move(a);
    CHECK(a.valueless_after_move());
    CHECK_FALSE(b.valueless_after_move());
}

// ---------------------------------------------------------------------------
// get_allocator
// ---------------------------------------------------------------------------

TEST_CASE("get_allocator returns the allocator used at construction")
{
    std::allocator<int> alloc;
    xyz::copy_on_write<int> x(std::allocator_arg, alloc, 7);
    CHECK(x.get_allocator() == alloc);
}

// ---------------------------------------------------------------------------
// use_count
// ---------------------------------------------------------------------------

TEST_CASE("use_count is 1 for a freshly constructed object")
{
    xyz::copy_on_write<int> x(0);
    CHECK(x.use_count() == 1);
}

TEST_CASE("use_count is 2 after copy construction")
{
    xyz::copy_on_write<int> a(0);
    xyz::copy_on_write<int> b(a);
    CHECK(a.use_count() == 2);
    CHECK(b.use_count() == 2);
}

TEST_CASE("use_count decrements when a sharing copy is destroyed")
{
    xyz::copy_on_write<int> a(0);
    {
        xyz::copy_on_write<int> b(a);
        CHECK(a.use_count() == 2);
    }
    CHECK(a.use_count() == 1);
}

TEST_CASE("use_count is 1 after move construction")
{
    xyz::copy_on_write<int> a(0);
    xyz::copy_on_write<int> b(std::move(a));
    CHECK(b.use_count() == 1);
}

// ---------------------------------------------------------------------------
// identical_to
// ---------------------------------------------------------------------------

TEST_CASE("identical_to is true for copies sharing the same buffer")
{
    xyz::copy_on_write<int> a(10);
    xyz::copy_on_write<int> b(a);
    CHECK(a.identical_to(b));
}

TEST_CASE("identical_to is false for independently constructed objects with equal values")
{
    xyz::copy_on_write<int> a(10);
    xyz::copy_on_write<int> b(10);
    CHECK_FALSE(a.identical_to(b));
}

TEST_CASE("identical_to is false after copy-on-write detach via modify")
{
    xyz::copy_on_write<int> a(10);
    xyz::copy_on_write<int> b(a);
    REQUIRE(a.identical_to(b));
    b.modify([](int& v) { v = 11; });
    CHECK_FALSE(a.identical_to(b));
    CHECK(*a == 10);
    CHECK(*b == 11);
}
