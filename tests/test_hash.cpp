#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <copy_on_write.hpp>

#include <functional>
#include <string>
#include <unordered_map>

// ---------------------------------------------------------------------------
// std::hash specialization
// ---------------------------------------------------------------------------

TEST_CASE("hash of live object equals hash of the stored value")
{
    xyz::copy_on_write<int> x(42);
    CHECK(std::hash<xyz::copy_on_write<int>>{}(x) == std::hash<int>{}(42));
}

TEST_CASE("hash of valueless object returns SIZE_MAX")
{
    xyz::copy_on_write<int> a(1);
    xyz::copy_on_write<int> b(std::move(a)); // a is now valueless
    CHECK(std::hash<xyz::copy_on_write<int>>{}(a) == static_cast<std::size_t>(-1));
}

TEST_CASE("two objects with equal values produce equal hashes")
{
    xyz::copy_on_write<std::string> a("hello");
    xyz::copy_on_write<std::string> b("hello");
    CHECK(std::hash<xyz::copy_on_write<std::string>>{}(a) ==
          std::hash<xyz::copy_on_write<std::string>>{}(b));
}

TEST_CASE("two objects with different values generally produce different hashes")
{
    xyz::copy_on_write<int> a(1);
    xyz::copy_on_write<int> b(2);
    // This is not guaranteed for all hash implementations, but holds for int.
    CHECK(std::hash<xyz::copy_on_write<int>>{}(a) !=
          std::hash<xyz::copy_on_write<int>>{}(b));
}

// ---------------------------------------------------------------------------
// Usable as key in std::unordered_map
// ---------------------------------------------------------------------------

TEST_CASE("copy_on_write can be used as key in std::unordered_map")
{
    std::unordered_map<xyz::copy_on_write<std::string>, int> m;
    xyz::copy_on_write<std::string> key1("alpha");
    xyz::copy_on_write<std::string> key2("beta");

    m[key1] = 1;
    m[key2] = 2;

    CHECK(m.at(key1) == 1);
    CHECK(m.at(key2) == 2);
    CHECK(m.count(xyz::copy_on_write<std::string>("alpha")) == 1u);
}
