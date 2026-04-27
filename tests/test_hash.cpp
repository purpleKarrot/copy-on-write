// SPDX-License-Identifier: BSL-1.0

#include <copy_on_write.hpp>

#include <gtest/gtest.h>

#include <unordered_map>
#include <unordered_set>

// ---------------------------------------------------------------------------
// std::hash matches the underlying type's hash
// ---------------------------------------------------------------------------

TEST(Hash, MatchesUnderlyingHash)
{
  xyz::copy_on_write<int> c{42};
  std::hash<xyz::copy_on_write<int>> cow_hash;
  std::hash<int> int_hash;
  EXPECT_EQ(cow_hash(c), int_hash(42));
}

TEST(Hash, MatchesUnderlyingHashString)
{
  xyz::copy_on_write<std::string> c{"hello"};
  std::hash<xyz::copy_on_write<std::string>> cow_hash;
  std::hash<std::string> str_hash;
  EXPECT_EQ(cow_hash(c), str_hash("hello"));
}

TEST(Hash, SameValueSameHash)
{
  xyz::copy_on_write<int> a{7};
  xyz::copy_on_write<int> b{7};
  std::hash<xyz::copy_on_write<int>> h;
  EXPECT_EQ(h(a), h(b));
}

// ---------------------------------------------------------------------------
// Valueless object hash
// ---------------------------------------------------------------------------

TEST(Hash, ValuelessReturnsMaxSizeT)
{
  xyz::copy_on_write<int> a{1};
  xyz::copy_on_write<int> b{std::move(a)};
  std::hash<xyz::copy_on_write<int>> h;
  EXPECT_EQ(h(a), static_cast<std::size_t>(-1));
}

// ---------------------------------------------------------------------------
// Works inside unordered containers
// ---------------------------------------------------------------------------

TEST(Hash, WorksInUnorderedSet)
{
  std::unordered_set<xyz::copy_on_write<int>> s;
  s.insert(xyz::copy_on_write<int>{1});
  s.insert(xyz::copy_on_write<int>{2});
  s.insert(xyz::copy_on_write<int>{1}); // duplicate

  EXPECT_EQ(s.size(), 2u);
  EXPECT_NE(s.find(xyz::copy_on_write<int>{1}), s.end());
  EXPECT_NE(s.find(xyz::copy_on_write<int>{2}), s.end());
  EXPECT_EQ(s.find(xyz::copy_on_write<int>{99}), s.end());
}

TEST(Hash, WorksInUnorderedMap)
{
  std::unordered_map<xyz::copy_on_write<std::string>, int> m;
  m[xyz::copy_on_write<std::string>{"a"}] = 1;
  m[xyz::copy_on_write<std::string>{"b"}] = 2;

  EXPECT_EQ(m.at(xyz::copy_on_write<std::string>{"a"}), 1);
  EXPECT_EQ(m.at(xyz::copy_on_write<std::string>{"b"}), 2);
}

TEST(Hash, CopiesHashIdentically)
{
  xyz::copy_on_write<int> a{100};
  xyz::copy_on_write<int> b{a};
  std::hash<xyz::copy_on_write<int>> h;
  EXPECT_EQ(h(a), h(b));
}
