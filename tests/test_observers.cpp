// SPDX-License-Identifier: BSL-1.0

#include <copy_on_write.hpp>

#include <gtest/gtest.h>

// ---------------------------------------------------------------------------
// operator*
// ---------------------------------------------------------------------------

TEST(Observers, Dereference)
{
  xyz::copy_on_write<int> c{42};
  EXPECT_EQ(*c, 42);
  static_assert(std::is_same_v<decltype(*c), int const&>);
}

TEST(Observers, DereferenceString)
{
  xyz::copy_on_write<std::string> c{"hello"};
  EXPECT_EQ(*c, "hello");
}

// ---------------------------------------------------------------------------
// operator->
// ---------------------------------------------------------------------------

TEST(Observers, Arrow)
{
  xyz::copy_on_write<std::string> c{"world"};
  EXPECT_EQ(c->size(), 5u);
}

TEST(Observers, ArrowConstPointerType)
{
  xyz::copy_on_write<std::string> c{"test"};
  static_assert(
    std::is_same_v<decltype(c.operator->()), std::string const*>);
}

// ---------------------------------------------------------------------------
// valueless_after_move
// ---------------------------------------------------------------------------

TEST(Observers, NotValuelessAfterConstruction)
{
  xyz::copy_on_write<int> c{1};
  EXPECT_FALSE(c.valueless_after_move());
}

TEST(Observers, ValuelessAfterMove)
{
  xyz::copy_on_write<int> a{1};
  xyz::copy_on_write<int> b{std::move(a)};
  EXPECT_TRUE(a.valueless_after_move());
  EXPECT_FALSE(b.valueless_after_move());
}

// ---------------------------------------------------------------------------
// get_allocator
// ---------------------------------------------------------------------------

TEST(Observers, GetAllocatorDefault)
{
  xyz::copy_on_write<int> c{5};
  std::allocator<int> expected;
  EXPECT_EQ(c.get_allocator(), expected);
}

// ---------------------------------------------------------------------------
// use_count
// ---------------------------------------------------------------------------

TEST(Observers, UseCountOnFreshObject)
{
  xyz::copy_on_write<int> c{0};
  EXPECT_EQ(c.use_count(), 1);
}

TEST(Observers, UseCountAfterCopy)
{
  xyz::copy_on_write<int> a{0};
  {
    xyz::copy_on_write<int> b{a};
    EXPECT_EQ(a.use_count(), 2);
    EXPECT_EQ(b.use_count(), 2);
  }
  EXPECT_EQ(a.use_count(), 1);
}

TEST(Observers, UseCountAfterMultipleCopies)
{
  xyz::copy_on_write<int> a{0};
  {
    xyz::copy_on_write<int> b{a};
    xyz::copy_on_write<int> c{a};
    EXPECT_EQ(a.use_count(), 3);
  }
  EXPECT_EQ(a.use_count(), 1);
}

// ---------------------------------------------------------------------------
// identical_to
// ---------------------------------------------------------------------------

TEST(Observers, IdenticalAfterCopy)
{
  xyz::copy_on_write<int> a{99};
  xyz::copy_on_write<int> b{a};
  EXPECT_TRUE(a.identical_to(b));
  EXPECT_TRUE(b.identical_to(a));
}

TEST(Observers, NotIdenticalForIndependentObjects)
{
  xyz::copy_on_write<int> a{1};
  xyz::copy_on_write<int> b{1};
  EXPECT_FALSE(a.identical_to(b));
}

TEST(Observers, NotIdenticalAfterModify)
{
  xyz::copy_on_write<int> a{5};
  xyz::copy_on_write<int> b{a};
  EXPECT_TRUE(a.identical_to(b));

  b.modify([](int& v) { v = 10; });
  EXPECT_FALSE(a.identical_to(b));
  EXPECT_EQ(*a, 5);
  EXPECT_EQ(*b, 10);
}
