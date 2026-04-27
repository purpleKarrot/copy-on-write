#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <copy_on_write.hpp>

#include <memory_resource>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Default constructor
// ---------------------------------------------------------------------------

TEST_CASE("default constructor value-initializes T")
{
    xyz::copy_on_write<int> x;
    CHECK(*x == 0);
    CHECK_FALSE(x.valueless_after_move());
}

TEST_CASE("default constructor with std::string")
{
    xyz::copy_on_write<std::string> x;
    CHECK(*x == "");
    CHECK_FALSE(x.valueless_after_move());
}

// ---------------------------------------------------------------------------
// Single-value constructor
// ---------------------------------------------------------------------------

TEST_CASE("single-value constructor stores the value")
{
    xyz::copy_on_write<int> x(42);
    CHECK(*x == 42);
}

TEST_CASE("single-value constructor with lvalue")
{
    std::string s = "hello";
    xyz::copy_on_write<std::string> x(s);
    CHECK(*x == "hello");
}

TEST_CASE("single-value constructor with rvalue")
{
    xyz::copy_on_write<std::string> x(std::string("world"));
    CHECK(*x == "world");
}

// ---------------------------------------------------------------------------
// In-place constructors
// ---------------------------------------------------------------------------

TEST_CASE("in-place constructor with multiple arguments")
{
    xyz::copy_on_write<std::string> x(std::in_place, 3u, 'a');
    CHECK(*x == "aaa");
}

TEST_CASE("in-place constructor with initializer list")
{
    xyz::copy_on_write<std::vector<int>> x(std::in_place, {1, 2, 3});
    CHECK((*x == std::vector<int>{1, 2, 3}));
}

TEST_CASE("in-place constructor with initializer list and extra args")
{
    // std::vector(ilist, alloc) — uses the init-list + allocator overload
    xyz::copy_on_write<std::vector<int>> x(std::in_place,
                                           std::initializer_list<int>{4, 5, 6},
                                           std::allocator<int>{});
    CHECK((*x == std::vector<int>{4, 5, 6}));
}

// ---------------------------------------------------------------------------
// allocator_arg constructors (default-value)
// ---------------------------------------------------------------------------

TEST_CASE("allocator_arg default constructor stores default value")
{
    xyz::copy_on_write<int> x(std::allocator_arg, std::allocator<int>{});
    CHECK(*x == 0);
    CHECK_FALSE(x.valueless_after_move());
}

TEST_CASE("allocator_arg single-value constructor")
{
    xyz::copy_on_write<int> x(std::allocator_arg, std::allocator<int>{}, 99);
    CHECK(*x == 99);
}

TEST_CASE("allocator_arg in-place constructor")
{
    xyz::copy_on_write<std::string> x(std::allocator_arg, std::allocator<std::string>{},
                                      std::in_place, 4u, 'z');
    CHECK(*x == "zzzz");
}

TEST_CASE("allocator_arg in-place initializer-list constructor")
{
    xyz::copy_on_write<std::vector<int>> x(std::allocator_arg,
                                           std::allocator<std::vector<int>>{},
                                           std::in_place, {7, 8, 9});
    CHECK((*x == std::vector<int>{7, 8, 9}));
}

// ---------------------------------------------------------------------------
// Copy constructor
// ---------------------------------------------------------------------------

TEST_CASE("copy constructor shares ownership")
{
    xyz::copy_on_write<int> a(10);
    xyz::copy_on_write<int> b(a);
    CHECK(a.use_count() == 2);
    CHECK(b.use_count() == 2);
    CHECK(a.identical_to(b));
    CHECK(*b == 10);
}

TEST_CASE("copy constructor preserves value")
{
    xyz::copy_on_write<std::string> a("copy me");
    xyz::copy_on_write<std::string> b(a);
    CHECK(*b == "copy me");
}

// ---------------------------------------------------------------------------
// Move constructor
// ---------------------------------------------------------------------------

TEST_CASE("move constructor transfers ownership, source becomes valueless")
{
    xyz::copy_on_write<int> a(7);
    xyz::copy_on_write<int> b(std::move(a));
    CHECK(a.valueless_after_move());
    CHECK(*b == 7);
}

TEST_CASE("move constructor use_count stays 1")
{
    xyz::copy_on_write<int> a(5);
    xyz::copy_on_write<int> b(std::move(a));
    CHECK(b.use_count() == 1);
}

// ---------------------------------------------------------------------------
// allocator_arg copy / move constructors
// ---------------------------------------------------------------------------

TEST_CASE("allocator_arg copy constructor with same allocator shares model")
{
    std::allocator<int> alloc;
    xyz::copy_on_write<int> a(std::allocator_arg, alloc, 21);
    xyz::copy_on_write<int> b(std::allocator_arg, alloc, a);
    CHECK(a.identical_to(b));
    CHECK(*b == 21);
}

TEST_CASE("allocator_arg move constructor with same allocator transfers model")
{
    std::allocator<int> alloc;
    xyz::copy_on_write<int> a(std::allocator_arg, alloc, 33);
    xyz::copy_on_write<int> b(std::allocator_arg, alloc, std::move(a));
    CHECK(a.valueless_after_move());
    CHECK(*b == 33);
}

// ---------------------------------------------------------------------------
// Deduction guides
// ---------------------------------------------------------------------------

TEST_CASE("deduction guide from value")
{
    auto x = xyz::copy_on_write(42);
    static_assert(std::is_same_v<decltype(x), xyz::copy_on_write<int>>);
    CHECK(*x == 42);
}

TEST_CASE("deduction guide from allocator_arg + value")
{
    auto x = xyz::copy_on_write(std::allocator_arg, std::allocator<double>{}, 3.14);
    static_assert(std::is_same_v<decltype(x), xyz::copy_on_write<double, std::allocator<double>>>);
    CHECK(*x == doctest::Approx(3.14));
}
