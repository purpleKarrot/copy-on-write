// SPDX-License-Identifier: BSL-1.0

#include <copy_on_write.hpp>

#include <gtest/gtest.h>

#include <vector>

// ---------------------------------------------------------------------------
// Default construction
// ---------------------------------------------------------------------------

TEST(Construction, DefaultConstruct)
{
  xyz::copy_on_write<int> c;
  EXPECT_FALSE(c.valueless_after_move());
  EXPECT_EQ(*c, 0);
  EXPECT_EQ(c.use_count(), 1);
}

TEST(Construction, DefaultConstructString)
{
  xyz::copy_on_write<std::string> c;
  EXPECT_FALSE(c.valueless_after_move());
  EXPECT_TRUE(c->empty());
}

// ---------------------------------------------------------------------------
// Value construction
// ---------------------------------------------------------------------------

TEST(Construction, ValueConstruct)
{
  xyz::copy_on_write<int> c{42};
  EXPECT_EQ(*c, 42);
}

TEST(Construction, ValueConstructString)
{
  xyz::copy_on_write<std::string> c{"hello"};
  EXPECT_EQ(*c, "hello");
}

TEST(Construction, DeductionGuide)
{
  xyz::copy_on_write c{3.14};
  static_assert(std::is_same_v<decltype(c), xyz::copy_on_write<double>>);
  EXPECT_DOUBLE_EQ(*c, 3.14);
}

// ---------------------------------------------------------------------------
// In-place construction
// ---------------------------------------------------------------------------

TEST(Construction, InPlaceArgs)
{
  xyz::copy_on_write<std::string> c{std::in_place, 5u, 'x'};
  EXPECT_EQ(*c, "xxxxx");
}

TEST(Construction, InPlaceInitializerList)
{
  xyz::copy_on_write<std::vector<int>> c{std::in_place, {1, 2, 3}};
  EXPECT_EQ((*c), (std::vector<int>{1, 2, 3}));
}

// ---------------------------------------------------------------------------
// Allocator-extended construction
// ---------------------------------------------------------------------------

TEST(Construction, AllocatorExtendedDefault)
{
  std::allocator<int> alloc;
  xyz::copy_on_write<int> c{std::allocator_arg, alloc};
  EXPECT_FALSE(c.valueless_after_move());
  EXPECT_EQ(*c, 0);
}

TEST(Construction, AllocatorExtendedValue)
{
  std::allocator<int> alloc;
  xyz::copy_on_write<int> c{std::allocator_arg, alloc, 99};
  EXPECT_EQ(*c, 99);
}

TEST(Construction, AllocatorExtendedInPlace)
{
  std::allocator<std::string> alloc;
  xyz::copy_on_write<std::string> c{std::allocator_arg, alloc, std::in_place, 3u, 'z'};
  EXPECT_EQ(*c, "zzz");
}

TEST(Construction, AllocatorExtendedInPlaceInitializerList)
{
  std::allocator<std::vector<int>> alloc;
  xyz::copy_on_write<std::vector<int>> c{std::allocator_arg, alloc, std::in_place, {4, 5, 6}};
  EXPECT_EQ((*c), (std::vector<int>{4, 5, 6}));
}

// ---------------------------------------------------------------------------
// Copy construction
// ---------------------------------------------------------------------------

TEST(Construction, CopyCtor)
{
  xyz::copy_on_write<int> a{7};
  xyz::copy_on_write<int> b{a};
  EXPECT_EQ(*b, 7);
  EXPECT_EQ(a.use_count(), 2);
  EXPECT_TRUE(b.identical_to(a));
}

TEST(Construction, CopyCtorSameAllocatorSharesStorage)
{
  xyz::copy_on_write<std::string> a{"shared"};
  xyz::copy_on_write<std::string> b{a};
  EXPECT_TRUE(b.identical_to(a));
  EXPECT_EQ(a.use_count(), 2);
}

TEST(Construction, AllocatorExtendedCopyCtorSameAllocator)
{
  std::allocator<int> alloc;
  xyz::copy_on_write<int> a{std::allocator_arg, alloc, 5};
  xyz::copy_on_write<int> b{std::allocator_arg, alloc, a};
  EXPECT_TRUE(b.identical_to(a));
  EXPECT_EQ(a.use_count(), 2);
}

TEST(Construction, AllocatorExtendedCopyCtorDifferentAllocatorDeepCopies)
{
  // Use PMR to get allocators that compare unequal
  std::array<std::byte, 1024> buf1;
  std::array<std::byte, 1024> buf2;
  std::pmr::monotonic_buffer_resource res1{buf1.data(), buf1.size()};
  std::pmr::monotonic_buffer_resource res2{buf2.data(), buf2.size()};
  std::pmr::polymorphic_allocator<int> alloc1{&res1};
  std::pmr::polymorphic_allocator<int> alloc2{&res2};

  xyz::copy_on_write<int, std::pmr::polymorphic_allocator<int>> a{std::allocator_arg, alloc1, 42};
  xyz::copy_on_write<int, std::pmr::polymorphic_allocator<int>> b{std::allocator_arg, alloc2, a};

  EXPECT_EQ(*b, 42);
  EXPECT_FALSE(b.identical_to(a));
  EXPECT_EQ(a.use_count(), 1);
}

// ---------------------------------------------------------------------------
// Move construction
// ---------------------------------------------------------------------------

TEST(Construction, MoveCtor)
{
  xyz::copy_on_write<int> a{10};
  xyz::copy_on_write<int> b{std::move(a)};
  EXPECT_TRUE(a.valueless_after_move());
  EXPECT_EQ(*b, 10);
  EXPECT_EQ(b.use_count(), 1);
}

TEST(Construction, MoveCtorLeavesSourceValueless)
{
  xyz::copy_on_write<std::string> a{"moved"};
  xyz::copy_on_write<std::string> b{std::move(a)};
  EXPECT_TRUE(a.valueless_after_move());
  EXPECT_EQ(*b, "moved");
}

TEST(Construction, AllocatorExtendedMoveCtor)
{
  std::allocator<int> alloc;
  xyz::copy_on_write<int> a{std::allocator_arg, alloc, 77};
  xyz::copy_on_write<int> b{std::allocator_arg, alloc, std::move(a)};
  EXPECT_TRUE(a.valueless_after_move());
  EXPECT_EQ(*b, 77);
}
