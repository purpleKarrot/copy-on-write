// SPDX-License-Identifier: BSL-1.0

#include <copy_on_write.hpp>
#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Default constructor
// ---------------------------------------------------------------------------

TEST(Construction, DefaultConstructorInt)
{
  xyz::copy_on_write<int> x;
  EXPECT_EQ(*x, 0);
  EXPECT_FALSE(x.valueless_after_move());
}

TEST(Construction, DefaultConstructorString)
{
  xyz::copy_on_write<std::string> x;
  EXPECT_EQ(*x, "");
  EXPECT_FALSE(x.valueless_after_move());
}

// ---------------------------------------------------------------------------
// Single-value constructor
// ---------------------------------------------------------------------------

TEST(Construction, SingleValueConstructorStores)
{
  xyz::copy_on_write<int> x(42);
  EXPECT_EQ(*x, 42);
}

TEST(Construction, SingleValueConstructorLvalue)
{
  std::string s = "hello";
  xyz::copy_on_write<std::string> x(s);
  EXPECT_EQ(*x, "hello");
}

TEST(Construction, SingleValueConstructorRvalue)
{
  xyz::copy_on_write<std::string> x(std::string("world"));
  EXPECT_EQ(*x, "world");
}

// ---------------------------------------------------------------------------
// In-place constructors
// ---------------------------------------------------------------------------

TEST(Construction, InPlaceMultipleArguments)
{
  xyz::copy_on_write<std::string> x(std::in_place, 3u, 'a');
  EXPECT_EQ(*x, "aaa");
}

TEST(Construction, InPlaceInitializerList)
{
  xyz::copy_on_write<std::vector<int>> x(std::in_place, {1, 2, 3});
  EXPECT_EQ(*x, (std::vector<int>{1, 2, 3}));
}

TEST(Construction, InPlaceInitializerListExtraArgs)
{
  // std::vector(ilist, alloc) — uses the init-list + allocator overload
  xyz::copy_on_write<std::vector<int>> x(std::in_place, std::initializer_list<int>{4, 5, 6},
                                         std::allocator<int>{});
  EXPECT_EQ(*x, (std::vector<int>{4, 5, 6}));
}

// ---------------------------------------------------------------------------
// allocator_arg constructors (default-value)
// ---------------------------------------------------------------------------

TEST(Construction, AllocArgDefaultConstructor)
{
  xyz::copy_on_write<int> x(std::allocator_arg, std::allocator<int>{});
  EXPECT_EQ(*x, 0);
  EXPECT_FALSE(x.valueless_after_move());
}

TEST(Construction, AllocArgSingleValueConstructor)
{
  xyz::copy_on_write<int> x(std::allocator_arg, std::allocator<int>{}, 99);
  EXPECT_EQ(*x, 99);
}

TEST(Construction, AllocArgInPlaceConstructor)
{
  xyz::copy_on_write<std::string> x(std::allocator_arg, std::allocator<std::string>{},
                                    std::in_place, 4u, 'z');
  EXPECT_EQ(*x, "zzzz");
}

TEST(Construction, AllocArgInPlaceInitializerList)
{
  xyz::copy_on_write<std::vector<int>> x(std::allocator_arg, std::allocator<std::vector<int>>{},
                                         std::in_place, {7, 8, 9});
  EXPECT_EQ(*x, (std::vector<int>{7, 8, 9}));
}

// ---------------------------------------------------------------------------
// Copy constructor
// ---------------------------------------------------------------------------

TEST(Construction, CopyConstructorSharesOwnership)
{
  xyz::copy_on_write<int> a(10);
  xyz::copy_on_write<int> b(a);
  EXPECT_EQ(a.use_count(), 2);
  EXPECT_EQ(b.use_count(), 2);
  EXPECT_TRUE(a.identical_to(b));
  EXPECT_EQ(*b, 10);
}

TEST(Construction, CopyConstructorPreservesValue)
{
  xyz::copy_on_write<std::string> a("copy me");
  xyz::copy_on_write<std::string> b(a);
  EXPECT_EQ(*b, "copy me");
}

TEST(Construction, CopyConstructorFromValueless)
{
  xyz::copy_on_write<int> a(42);
  xyz::copy_on_write<int> sink(std::move(a)); // a is now valueless
  xyz::copy_on_write<int> b(a);              // copy-construct from valueless source
  EXPECT_TRUE(a.valueless_after_move());
  EXPECT_TRUE(b.valueless_after_move());
}

// ---------------------------------------------------------------------------
// Move constructor
// ---------------------------------------------------------------------------

TEST(Construction, MoveConstructorTransfersOwnership)
{
  xyz::copy_on_write<int> a(7);
  xyz::copy_on_write<int> b(std::move(a));
  EXPECT_TRUE(a.valueless_after_move());
  EXPECT_EQ(*b, 7);
}

TEST(Construction, MoveConstructorUseCountStaysOne)
{
  xyz::copy_on_write<int> a(5);
  xyz::copy_on_write<int> b(std::move(a));
  EXPECT_EQ(b.use_count(), 1);
}

// ---------------------------------------------------------------------------
// allocator_arg copy / move constructors
// ---------------------------------------------------------------------------

TEST(Construction, AllocArgCopyConstructorSameAllocatorSharesModel)
{
  std::allocator<int> alloc;
  xyz::copy_on_write<int> a(std::allocator_arg, alloc, 21);
  xyz::copy_on_write<int> b(std::allocator_arg, alloc, a);
  EXPECT_TRUE(a.identical_to(b));
  EXPECT_EQ(*b, 21);
}

TEST(Construction, AllocArgCopyConstructorFromValueless)
{
  xyz::copy_on_write<int> a(42);
  xyz::copy_on_write<int> sink(std::move(a)); // a is now valueless
  std::allocator<int> alloc;
  xyz::copy_on_write<int> b(std::allocator_arg, alloc, a); // copy from valueless
  EXPECT_TRUE(b.valueless_after_move());
}

TEST(Construction, AllocArgMoveConstructorSameAllocatorTransfersModel)
{
  std::allocator<int> alloc;
  xyz::copy_on_write<int> a(std::allocator_arg, alloc, 33);
  xyz::copy_on_write<int> b(std::allocator_arg, alloc, std::move(a));
  EXPECT_TRUE(a.valueless_after_move());
  EXPECT_EQ(*b, 33);
}

TEST(Construction, AllocArgMoveConstructorFromValueless)
{
  xyz::copy_on_write<int> a(42);
  xyz::copy_on_write<int> sink(std::move(a)); // a is now valueless
  std::allocator<int> alloc;
  xyz::copy_on_write<int> b(std::allocator_arg, alloc, std::move(a)); // move from valueless
  EXPECT_TRUE(b.valueless_after_move());
}

// ---------------------------------------------------------------------------
// Deduction guides
// ---------------------------------------------------------------------------

TEST(Construction, DeductionGuideFromValue)
{
  auto x = xyz::copy_on_write(42);
  static_assert(std::is_same_v<decltype(x), xyz::copy_on_write<int>>);
  EXPECT_EQ(*x, 42);
}

TEST(Construction, DeductionGuideFromAllocArgValue)
{
  auto x = xyz::copy_on_write(std::allocator_arg, std::allocator<double>{}, 3.14);
  static_assert(std::is_same_v<decltype(x), xyz::copy_on_write<double, std::allocator<double>>>);
  EXPECT_DOUBLE_EQ(*x, 3.14);
}

// ---------------------------------------------------------------------------
// Exception safety: _make_model catch path
// ---------------------------------------------------------------------------

TEST(Construction, ExceptionInValueConstructorPropagates)
{
  struct ThrowOnConstruct
  {
    ThrowOnConstruct() { throw std::runtime_error("throws"); }
  };

  EXPECT_THROW(xyz::copy_on_write<ThrowOnConstruct>(), std::runtime_error);
}
