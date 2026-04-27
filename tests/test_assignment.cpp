// SPDX-License-Identifier: BSL-1.0

#include <copy_on_write.hpp>

#include <gtest/gtest.h>

// ---------------------------------------------------------------------------
// Copy assignment
// ---------------------------------------------------------------------------

TEST(Assignment, CopyAssignSharesStorage)
{
  xyz::copy_on_write<int> a{5};
  xyz::copy_on_write<int> b{0};
  b = a;
  EXPECT_TRUE(b.identical_to(a));
  EXPECT_EQ(a.use_count(), 2);
}

TEST(Assignment, CopyAssignUpdatesValue)
{
  xyz::copy_on_write<int> a{42};
  xyz::copy_on_write<int> b{0};
  b = a;
  EXPECT_EQ(*b, 42);
}

TEST(Assignment, CopyAssignSelf)
{
  xyz::copy_on_write<int> a{7};
  // Self-assignment must be safe
  // Use a reference to avoid compiler warnings
  auto& ref = a;
  a = ref;
  EXPECT_EQ(*a, 7);
  EXPECT_EQ(a.use_count(), 1);
}

TEST(Assignment, CopyAssignFromValueless)
{
  xyz::copy_on_write<int> a{1};
  xyz::copy_on_write<int> b{std::move(a)};
  xyz::copy_on_write<int> c{99};
  c = a; // a is valueless
  EXPECT_TRUE(c.valueless_after_move());
}

// ---------------------------------------------------------------------------
// Move assignment
// ---------------------------------------------------------------------------

TEST(Assignment, MoveAssignStealsStorage)
{
  xyz::copy_on_write<int> a{10};
  xyz::copy_on_write<int> b{0};
  b = std::move(a);
  EXPECT_TRUE(a.valueless_after_move());
  EXPECT_EQ(*b, 10);
}

TEST(Assignment, MoveAssignSelf)
{
  xyz::copy_on_write<int> a{3};
  auto& ref = a;
  a = std::move(ref);
  // After self-move-assign the object should still hold the value
  // (the implementation guards against self-assignment)
  EXPECT_EQ(*a, 3);
}

TEST(Assignment, MoveAssignFromValueless)
{
  xyz::copy_on_write<int> a{1};
  xyz::copy_on_write<int> b{std::move(a)};
  xyz::copy_on_write<int> c{99};
  c = std::move(a); // a is already valueless
  EXPECT_TRUE(c.valueless_after_move());
}

// ---------------------------------------------------------------------------
// Value assignment (operator=(U&&))
// ---------------------------------------------------------------------------

TEST(Assignment, ValueAssignExclusiveModifiesInPlace)
{
  xyz::copy_on_write<int> c{1};
  int const* ptr = &*c;
  c = 99;
  // When exclusive, the stored object is updated in-place (same address)
  EXPECT_EQ(*c, 99);
  EXPECT_EQ(&*c, ptr);
}

TEST(Assignment, ValueAssignSharedAllocatesNew)
{
  xyz::copy_on_write<int> a{1};
  xyz::copy_on_write<int> b{a};
  EXPECT_EQ(a.use_count(), 2);

  b = 55;

  EXPECT_EQ(*b, 55);
  EXPECT_EQ(*a, 1);
  EXPECT_EQ(a.use_count(), 1);
  EXPECT_FALSE(a.identical_to(b));
}

TEST(Assignment, ValueAssignString)
{
  xyz::copy_on_write<std::string> c{"old"};
  c = std::string{"new"};
  EXPECT_EQ(*c, "new");
}

// ---------------------------------------------------------------------------
// Chain of copies and assignments
// ---------------------------------------------------------------------------

TEST(Assignment, UseCountDecreasesOnReassign)
{
  xyz::copy_on_write<int> a{1};
  xyz::copy_on_write<int> b{a};
  EXPECT_EQ(a.use_count(), 2);

  b = xyz::copy_on_write<int>{2};
  EXPECT_EQ(a.use_count(), 1);
  EXPECT_EQ(*b, 2);
}
