// SPDX-License-Identifier: BSL-1.0

#include <copy_on_write.hpp>
#include <gtest/gtest.h>

#include <string>

// ---------------------------------------------------------------------------
// operator*
// ---------------------------------------------------------------------------

TEST(Observers, DerefReturnsConstRef)
{
  xyz::copy_on_write<int> x(42);
  static_assert(std::is_same_v<decltype(*x), int const&>);
  EXPECT_EQ(*x, 42);
}

TEST(Observers, DerefReflectsValueChangesViaModify)
{
  xyz::copy_on_write<int> x(1);
  x.modify([](int& v) { v = 99; });
  EXPECT_EQ(*x, 99);
}

// ---------------------------------------------------------------------------
// operator->
// ---------------------------------------------------------------------------

TEST(Observers, ArrowProvidesConstAccessToMember)
{
  xyz::copy_on_write<std::string> x("hello");
  EXPECT_EQ(x->size(), 5u);
  EXPECT_EQ(x->front(), 'h');
}

// ---------------------------------------------------------------------------
// valueless_after_move
// ---------------------------------------------------------------------------

TEST(Observers, ValuelessAfterMoveIsFalseForLiveObject)
{
  xyz::copy_on_write<int> x(0);
  EXPECT_FALSE(x.valueless_after_move());
}

TEST(Observers, ValuelessAfterMoveIsTrueAfterMoveConstruction)
{
  xyz::copy_on_write<int> a(5);
  auto b = std::move(a);
  EXPECT_TRUE(a.valueless_after_move());
  EXPECT_FALSE(b.valueless_after_move());
}

TEST(Observers, ValuelessAfterMoveIsTrueAfterMoveAssignment)
{
  xyz::copy_on_write<int> a(5);
  xyz::copy_on_write<int> b(0);
  b = std::move(a);
  EXPECT_TRUE(a.valueless_after_move());
  EXPECT_FALSE(b.valueless_after_move());
}

// ---------------------------------------------------------------------------
// get_allocator
// ---------------------------------------------------------------------------

TEST(Observers, GetAllocatorReturnsConstructionAllocator)
{
  std::allocator<int> alloc;
  xyz::copy_on_write<int> x(std::allocator_arg, alloc, 7);
  EXPECT_EQ(x.get_allocator(), alloc);
}

// ---------------------------------------------------------------------------
// use_count
// ---------------------------------------------------------------------------

TEST(Observers, UseCountIsOneForFreshObject)
{
  xyz::copy_on_write<int> x(0);
  EXPECT_EQ(x.use_count(), 1);
}

TEST(Observers, UseCountIsTwoAfterCopyConstruction)
{
  xyz::copy_on_write<int> a(0);
  xyz::copy_on_write<int> b(a);
  EXPECT_EQ(a.use_count(), 2);
  EXPECT_EQ(b.use_count(), 2);
}

TEST(Observers, UseCountDecrementsWhenSharingCopyIsDestroyed)
{
  xyz::copy_on_write<int> a(0);
  {
    xyz::copy_on_write<int> b(a);
    EXPECT_EQ(a.use_count(), 2);
  }
  EXPECT_EQ(a.use_count(), 1);
}

TEST(Observers, UseCountIsOneAfterMoveConstruction)
{
  xyz::copy_on_write<int> a(0);
  xyz::copy_on_write<int> b(std::move(a));
  EXPECT_EQ(b.use_count(), 1);
}

// ---------------------------------------------------------------------------
// identical_to
// ---------------------------------------------------------------------------

TEST(Observers, IdenticalToIsTrueForCopiesSharingBuffer)
{
  xyz::copy_on_write<int> a(10);
  xyz::copy_on_write<int> b(a);
  EXPECT_TRUE(a.identical_to(b));
}

TEST(Observers, IdenticalToIsFalseForIndependentlyConstructedObjects)
{
  xyz::copy_on_write<int> a(10);
  xyz::copy_on_write<int> b(10);
  EXPECT_FALSE(a.identical_to(b));
}

TEST(Observers, IdenticalToIsFalseAfterDetachViaModify)
{
  xyz::copy_on_write<int> a(10);
  xyz::copy_on_write<int> b(a);
  ASSERT_TRUE(a.identical_to(b));
  b.modify([](int& v) { v = 11; });
  EXPECT_FALSE(a.identical_to(b));
  EXPECT_EQ(*a, 10);
  EXPECT_EQ(*b, 11);
}
