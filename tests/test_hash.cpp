#include <gtest/gtest.h>

#include <copy_on_write.hpp>

#include <functional>
#include <string>
#include <unordered_map>

// ---------------------------------------------------------------------------
// std::hash specialization
// ---------------------------------------------------------------------------

TEST(Hash, HashOfLiveObjectEqualsHashOfStoredValue)
{
    xyz::copy_on_write<int> x(42);
    EXPECT_EQ(std::hash<xyz::copy_on_write<int>>{}(x), std::hash<int>{}(42));
}

TEST(Hash, HashOfValuelessObjectReturnsSizeMax)
{
    xyz::copy_on_write<int> a(1);
    xyz::copy_on_write<int> b(std::move(a)); // a is now valueless
    EXPECT_EQ(std::hash<xyz::copy_on_write<int>>{}(a), static_cast<std::size_t>(-1));
}

TEST(Hash, TwoObjectsWithEqualValuesProduceEqualHashes)
{
    xyz::copy_on_write<std::string> a("hello");
    xyz::copy_on_write<std::string> b("hello");
    EXPECT_EQ(std::hash<xyz::copy_on_write<std::string>>{}(a),
              std::hash<xyz::copy_on_write<std::string>>{}(b));
}

TEST(Hash, TwoObjectsWithDifferentValuesProduceDifferentHashes)
{
    xyz::copy_on_write<int> a(1);
    xyz::copy_on_write<int> b(2);
    // This is not guaranteed for all hash implementations, but holds for int.
    EXPECT_NE(std::hash<xyz::copy_on_write<int>>{}(a),
              std::hash<xyz::copy_on_write<int>>{}(b));
}

// ---------------------------------------------------------------------------
// Usable as key in std::unordered_map
// ---------------------------------------------------------------------------

TEST(Hash, UsableAsKeyInUnorderedMap)
{
    std::unordered_map<xyz::copy_on_write<std::string>, int> m;
    xyz::copy_on_write<std::string> key1("alpha");
    xyz::copy_on_write<std::string> key2("beta");

    m[key1] = 1;
    m[key2] = 2;

    EXPECT_EQ(m.at(key1), 1);
    EXPECT_EQ(m.at(key2), 2);
    EXPECT_EQ(m.count(xyz::copy_on_write<std::string>("alpha")), 1u);
}

