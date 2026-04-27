// SPDX-License-Identifier: BSL-1.0

#include <copy_on_write.hpp>

#include <gtest/gtest.h>

#include <memory_resource>
#include <scoped_allocator>

// ---------------------------------------------------------------------------
// Tracking allocator — counts allocations and deallocations
// ---------------------------------------------------------------------------

struct alloc_counters
{
  long allocs = 0;
  long deallocs = 0;
};

template <typename T>
struct tracking_allocator
{
  using value_type = T;
  using propagate_on_container_copy_assignment = std::true_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap = std::true_type;

  std::shared_ptr<alloc_counters> counts = std::make_shared<alloc_counters>();

  tracking_allocator() = default;

  template <typename U>
  tracking_allocator(tracking_allocator<U> const& other) noexcept
    : counts{other.counts}
  {
  }

  T* allocate(std::size_t n)
  {
    ++counts->allocs;
    return std::allocator<T>{}.allocate(n);
  }

  void deallocate(T* p, std::size_t n)
  {
    ++counts->deallocs;
    std::allocator<T>{}.deallocate(p, n);
  }

  bool operator==(tracking_allocator const& other) const noexcept
  {
    return counts == other.counts;
  }
};

// ---------------------------------------------------------------------------
// PMR variant
// ---------------------------------------------------------------------------

TEST(Allocator, PmrDefaultConstruct)
{
  xyz::pmr::copy_on_write<int> c;
  EXPECT_FALSE(c.valueless_after_move());
  EXPECT_EQ(*c, 0);
}

TEST(Allocator, PmrValueConstruct)
{
  xyz::pmr::copy_on_write<int> c{42};
  EXPECT_EQ(*c, 42);
}

TEST(Allocator, PmrCopyCtorSameResource)
{
  std::pmr::monotonic_buffer_resource res;
  std::pmr::polymorphic_allocator<int> alloc{&res};

  xyz::pmr::copy_on_write<int> a{std::allocator_arg, alloc, 7};
  // Use allocator-extended copy ctor with the same allocator to share storage
  xyz::pmr::copy_on_write<int> b{std::allocator_arg, alloc, a};

  EXPECT_TRUE(a.identical_to(b));
  EXPECT_EQ(a.use_count(), 2);
}

TEST(Allocator, PmrPlainCopyCtorUsesDefaultResource)
{
  std::pmr::monotonic_buffer_resource res;
  std::pmr::polymorphic_allocator<int> alloc{&res};

  xyz::pmr::copy_on_write<int> a{std::allocator_arg, alloc, 7};
  // Plain copy ctor calls select_on_container_copy_construction which for PMR
  // returns the default resource allocator → allocators differ → deep copy
  xyz::pmr::copy_on_write<int> b{a};

  EXPECT_EQ(*b, 7);
  EXPECT_FALSE(a.identical_to(b));
  EXPECT_EQ(a.use_count(), 1);
}

TEST(Allocator, PmrCopyCtorDifferentResource)
{
  std::array<std::byte, 512> buf1, buf2;
  std::pmr::monotonic_buffer_resource res1{buf1.data(), buf1.size()};
  std::pmr::monotonic_buffer_resource res2{buf2.data(), buf2.size()};
  std::pmr::polymorphic_allocator<int> alloc1{&res1};
  std::pmr::polymorphic_allocator<int> alloc2{&res2};

  xyz::pmr::copy_on_write<int> a{std::allocator_arg, alloc1, 3};
  xyz::pmr::copy_on_write<int> b{std::allocator_arg, alloc2, a};

  EXPECT_EQ(*b, 3);
  EXPECT_FALSE(b.identical_to(a));
  EXPECT_EQ(a.use_count(), 1);
}

// ---------------------------------------------------------------------------
// Tracking allocator — allocation counting
// ---------------------------------------------------------------------------

TEST(Allocator, TrackingAllocatorCountsAllocations)
{
  tracking_allocator<int> alloc;
  EXPECT_EQ(alloc.counts->allocs, 0);

  {
    xyz::copy_on_write<int, tracking_allocator<int>> c{std::allocator_arg, alloc, 1};
    EXPECT_EQ(alloc.counts->allocs, 1);
  }
  // Destructor should have deallocated
  EXPECT_EQ(alloc.counts->deallocs, 1);
}

TEST(Allocator, TrackingAllocatorCopySharesNoNewAllocation)
{
  tracking_allocator<int> alloc;

  xyz::copy_on_write<int, tracking_allocator<int>> a{std::allocator_arg, alloc, 10};
  long allocs_before = alloc.counts->allocs;

  xyz::copy_on_write<int, tracking_allocator<int>> b{a}; // same alloc: just ref-count bump
  EXPECT_EQ(alloc.counts->allocs, allocs_before); // no new allocation
  EXPECT_TRUE(a.identical_to(b));
}

TEST(Allocator, TrackingAllocatorModifyAllocatesNew)
{
  tracking_allocator<int> alloc;
  xyz::copy_on_write<int, tracking_allocator<int>> a{std::allocator_arg, alloc, 5};
  xyz::copy_on_write<int, tracking_allocator<int>> b{a};

  long allocs_before = alloc.counts->allocs;
  b.modify([](int& v) { v = 99; }); // triggers COW copy
  EXPECT_EQ(alloc.counts->allocs, allocs_before + 1);
  EXPECT_EQ(*b, 99);
  EXPECT_EQ(*a, 5);
}

// ---------------------------------------------------------------------------
// Propagation on copy assignment (POCCA = true_type)
// ---------------------------------------------------------------------------

TEST(Allocator, PropagateOnCopyAssignment)
{
  tracking_allocator<int> alloc1;
  tracking_allocator<int> alloc2;
  alloc2.counts = std::make_shared<alloc_counters>();

  xyz::copy_on_write<int, tracking_allocator<int>> a{std::allocator_arg, alloc1, 1};
  xyz::copy_on_write<int, tracking_allocator<int>> b{std::allocator_arg, alloc2, 2};

  b = a;

  EXPECT_EQ(*b, 1);
  // After POCCA copy assignment the allocator propagates, so b should share
  // storage with a (same tracking block → same alloc id after propagation).
  EXPECT_TRUE(b.identical_to(a));
}

// ---------------------------------------------------------------------------
// Propagation on move assignment (POCMA = true_type)
// ---------------------------------------------------------------------------

TEST(Allocator, PropagateOnMoveAssignment)
{
  tracking_allocator<int> alloc1;
  tracking_allocator<int> alloc2;
  alloc2.counts = std::make_shared<alloc_counters>();

  xyz::copy_on_write<int, tracking_allocator<int>> a{std::allocator_arg, alloc1, 7};
  xyz::copy_on_write<int, tracking_allocator<int>> b{std::allocator_arg, alloc2, 8};

  b = std::move(a);

  EXPECT_TRUE(a.valueless_after_move());
  EXPECT_EQ(*b, 7);
}

// ---------------------------------------------------------------------------
// Propagation on swap (POCS = true_type)
// ---------------------------------------------------------------------------

TEST(Allocator, PropagateOnSwap)
{
  tracking_allocator<int> alloc1;
  tracking_allocator<int> alloc2;
  alloc2.counts = std::make_shared<alloc_counters>();

  xyz::copy_on_write<int, tracking_allocator<int>> a{std::allocator_arg, alloc1, 11};
  xyz::copy_on_write<int, tracking_allocator<int>> b{std::allocator_arg, alloc2, 22};

  a.swap(b);

  EXPECT_EQ(*a, 22);
  EXPECT_EQ(*b, 11);
}

// ---------------------------------------------------------------------------
// Allocator-extended move constructor — mismatched allocators deep-copy
// ---------------------------------------------------------------------------

TEST(Allocator, AllocatorExtendedMoveMismatchDeepCopies)
{
  std::array<std::byte, 512> buf1, buf2;
  std::pmr::monotonic_buffer_resource res1{buf1.data(), buf1.size()};
  std::pmr::monotonic_buffer_resource res2{buf2.data(), buf2.size()};
  std::pmr::polymorphic_allocator<int> alloc1{&res1};
  std::pmr::polymorphic_allocator<int> alloc2{&res2};

  xyz::pmr::copy_on_write<int> a{std::allocator_arg, alloc1, 55};
  xyz::pmr::copy_on_write<int> b{std::allocator_arg, alloc2, std::move(a)};

  EXPECT_TRUE(a.valueless_after_move());
  EXPECT_EQ(*b, 55);
}
