// SPDX-License-Identifier: BSL-1.0

#include <copy_on_write.hpp>
#include <gtest/gtest.h>

#include <memory>
#include <memory_resource>
#include <string>

namespace {

// ---------------------------------------------------------------------------
// Tracking allocator
// ---------------------------------------------------------------------------

template <typename T>
struct tracking_allocator
{
  using value_type = T;

  // Propagation traits: all false by default so we can override per test.
  using propagate_on_container_copy_assignment = std::false_type;
  using propagate_on_container_move_assignment = std::false_type;
  using propagate_on_container_swap = std::false_type;
  using is_always_equal = std::false_type;

  int* alloc_count = nullptr;
  int* dealloc_count = nullptr;
  int id = 0;

  tracking_allocator() = default;

  tracking_allocator(int* ac, int* dc, int id_)
    : alloc_count(ac)
    , dealloc_count(dc)
    , id(id_)
  {
  }

  template <typename U>
  tracking_allocator(tracking_allocator<U> const& o) noexcept
    : alloc_count(o.alloc_count)
    , dealloc_count(o.dealloc_count)
    , id(o.id)
  {
  }

  T* allocate(std::size_t n)
  {
    if (alloc_count) {
      ++(*alloc_count);
    }
    return std::allocator<T>{}.allocate(n);
  }

  void deallocate(T* p, std::size_t n) noexcept
  {
    if (dealloc_count) {
      ++(*dealloc_count);
    }
    std::allocator<T>{}.deallocate(p, n);
  }

  bool operator==(tracking_allocator const& o) const noexcept { return id == o.id; }
};

// ---------------------------------------------------------------------------
// Propagating variants (POCCA / POCMA / POCS = true)
// ---------------------------------------------------------------------------

template <typename T>
struct pocca_allocator : tracking_allocator<T>
{
  using propagate_on_container_copy_assignment = std::true_type;

  template <typename U>
  struct rebind
  {
    using other = pocca_allocator<U>;
  };

  using tracking_allocator<T>::tracking_allocator;

  template <typename U>
  pocca_allocator(pocca_allocator<U> const& o) noexcept
    : tracking_allocator<T>(o)
  {
  }
};

template <typename T>
struct pocma_allocator : tracking_allocator<T>
{
  using propagate_on_container_move_assignment = std::true_type;

  template <typename U>
  struct rebind
  {
    using other = pocma_allocator<U>;
  };

  using tracking_allocator<T>::tracking_allocator;

  template <typename U>
  pocma_allocator(pocma_allocator<U> const& o) noexcept
    : tracking_allocator<T>(o)
  {
  }
};

template <typename T>
struct pocs_allocator : tracking_allocator<T>
{
  using propagate_on_container_swap = std::true_type;

  template <typename U>
  struct rebind
  {
    using other = pocs_allocator<U>;
  };

  using tracking_allocator<T>::tracking_allocator;

  template <typename U>
  pocs_allocator(pocs_allocator<U> const& o) noexcept
    : tracking_allocator<T>(o)
  {
  }
};

} // namespace

// ---------------------------------------------------------------------------
// Basic allocation tracking
// ---------------------------------------------------------------------------

TEST(Allocator, ExactlyOneAllocationPerConstructedObject)
{
  int allocs = 0, deallocs = 0;
  tracking_allocator<int> ta(&allocs, &deallocs, 1);
  {
    xyz::copy_on_write<int, tracking_allocator<int>> x(std::allocator_arg, ta, 42);
    EXPECT_EQ(allocs, 1);
  }
  EXPECT_EQ(deallocs, 1);
}

TEST(Allocator, DestructorDeallocatesWhenUseCountDropsToZero)
{
  int allocs = 0, deallocs = 0;
  tracking_allocator<int> ta(&allocs, &deallocs, 1);
  {
    xyz::copy_on_write<int, tracking_allocator<int>> a(std::allocator_arg, ta, 1);
    {
      xyz::copy_on_write<int, tracking_allocator<int>> b(a);
      EXPECT_EQ(deallocs, 0);
    }
    EXPECT_EQ(deallocs, 0); // a still alive
  }
  EXPECT_EQ(deallocs, 1); // now freed
}

// ---------------------------------------------------------------------------
// Copy with same vs. different allocator
// ---------------------------------------------------------------------------

TEST(Allocator, CopyWithSameAllocatorSharesModel)
{
  int allocs = 0, deallocs = 0;
  tracking_allocator<int> ta(&allocs, &deallocs, 1);
  xyz::copy_on_write<int, tracking_allocator<int>> a(std::allocator_arg, ta, 5);
  int allocs_before = allocs;
  xyz::copy_on_write<int, tracking_allocator<int>> b(std::allocator_arg, ta, a);
  EXPECT_EQ(allocs, allocs_before); // no new allocation
  EXPECT_TRUE(a.identical_to(b));
}

TEST(Allocator, CopyWithDifferentAllocatorAllocatesNewModel)
{
  int allocs1 = 0, deallocs1 = 0;
  int allocs2 = 0, deallocs2 = 0;
  tracking_allocator<int> ta1(&allocs1, &deallocs1, 1);
  tracking_allocator<int> ta2(&allocs2, &deallocs2, 2);

  xyz::copy_on_write<int, tracking_allocator<int>> a(std::allocator_arg, ta1, 5);
  xyz::copy_on_write<int, tracking_allocator<int>> b(std::allocator_arg, ta2, a);

  EXPECT_EQ(allocs2, 1);
  EXPECT_FALSE(a.identical_to(b));
  EXPECT_EQ(*b, 5);
}

// ---------------------------------------------------------------------------
// POCCA
// ---------------------------------------------------------------------------

TEST(Allocator, PoccaAllocatorIsPropagatedOnCopyAssignment)
{
  int allocs1 = 0, deallocs1 = 0;
  int allocs2 = 0, deallocs2 = 0;
  pocca_allocator<int> ta1(&allocs1, &deallocs1, 1);
  pocca_allocator<int> ta2(&allocs2, &deallocs2, 2);

  xyz::copy_on_write<int, pocca_allocator<int>> a(std::allocator_arg, ta1, 10);
  xyz::copy_on_write<int, pocca_allocator<int>> b(std::allocator_arg, ta2, 20);

  b = a;

  // After copy assignment with POCCA, b should use a's allocator.
  EXPECT_EQ(b.get_allocator(), ta1);
  EXPECT_EQ(*b, 10);
}

// ---------------------------------------------------------------------------
// POCMA
// ---------------------------------------------------------------------------

TEST(Allocator, PocmaAllocatorIsPropagatedOnMoveAssignment)
{
  int allocs1 = 0, deallocs1 = 0;
  int allocs2 = 0, deallocs2 = 0;
  pocma_allocator<int> ta1(&allocs1, &deallocs1, 1);
  pocma_allocator<int> ta2(&allocs2, &deallocs2, 2);

  xyz::copy_on_write<int, pocma_allocator<int>> a(std::allocator_arg, ta1, 10);
  xyz::copy_on_write<int, pocma_allocator<int>> b(std::allocator_arg, ta2, 20);

  b = std::move(a);

  EXPECT_EQ(b.get_allocator(), ta1);
  EXPECT_EQ(*b, 10);
  EXPECT_TRUE(a.valueless_after_move());
}

TEST(Allocator, WithoutPocmaMoveAssignmentMovesValue)
{
  int allocs1 = 0, deallocs1 = 0;
  int allocs2 = 0, deallocs2 = 0;
  tracking_allocator<int> ta1(&allocs1, &deallocs1, 1);
  tracking_allocator<int> ta2(&allocs2, &deallocs2, 2);

  xyz::copy_on_write<int, tracking_allocator<int>> a(std::allocator_arg, ta1, 55);
  xyz::copy_on_write<int, tracking_allocator<int>> b(std::allocator_arg, ta2, 0);

  b = std::move(a);

  EXPECT_EQ(*b, 55);
  // b keeps its allocator (POCMA is false)
  EXPECT_EQ(b.get_allocator(), ta2);
}

// ---------------------------------------------------------------------------
// POCS
// ---------------------------------------------------------------------------

TEST(Allocator, PocsAllocatorsAreSwappedOnMemberSwap)
{
  int allocs1 = 0, deallocs1 = 0;
  int allocs2 = 0, deallocs2 = 0;
  pocs_allocator<int> ta1(&allocs1, &deallocs1, 1);
  pocs_allocator<int> ta2(&allocs2, &deallocs2, 2);

  xyz::copy_on_write<int, pocs_allocator<int>> a(std::allocator_arg, ta1, 111);
  xyz::copy_on_write<int, pocs_allocator<int>> b(std::allocator_arg, ta2, 222);

  a.swap(b);

  EXPECT_EQ(*a, 222);
  EXPECT_EQ(*b, 111);
  EXPECT_EQ(a.get_allocator(), ta2);
  EXPECT_EQ(b.get_allocator(), ta1);
}

// ---------------------------------------------------------------------------
// xyz::pmr::copy_on_write
// ---------------------------------------------------------------------------

TEST(Allocator, PmrCopyOnWriteWorksWithMonotonicBufferResource)
{
  std::array<std::byte, 1024> buf;
  std::pmr::monotonic_buffer_resource pool(buf.data(), buf.size());
  std::pmr::polymorphic_allocator<int> pmr_alloc(&pool);

  xyz::pmr::copy_on_write<int> x(std::allocator_arg, pmr_alloc, 42);
  EXPECT_EQ(*x, 42);
  EXPECT_FALSE(x.valueless_after_move());
}

TEST(Allocator, PmrCopyOnWriteCopySharesModel)
{
  std::array<std::byte, 1024> buf;
  std::pmr::monotonic_buffer_resource pool(buf.data(), buf.size());
  std::pmr::polymorphic_allocator<int> pmr_alloc(&pool);

  xyz::pmr::copy_on_write<int> a(std::allocator_arg, pmr_alloc, 7);
  xyz::pmr::copy_on_write<int> b(std::allocator_arg, pmr_alloc, a);

  EXPECT_TRUE(a.identical_to(b));
  EXPECT_EQ(*b, 7);
}

// ---------------------------------------------------------------------------
// allocator_arg move constructor — else branch (different, non-equal allocators)
// ---------------------------------------------------------------------------

TEST(Allocator, AllocArgMoveConstructorDifferentAllocatorAllocatesNewModel)
{
  int allocs1 = 0, deallocs1 = 0;
  int allocs2 = 0, deallocs2 = 0;
  tracking_allocator<int> ta1(&allocs1, &deallocs1, 1);
  tracking_allocator<int> ta2(&allocs2, &deallocs2, 2);

  xyz::copy_on_write<int, tracking_allocator<int>> a(std::allocator_arg, ta1, 99);
  xyz::copy_on_write<int, tracking_allocator<int>> b(std::allocator_arg, ta2, std::move(a));

  EXPECT_TRUE(a.valueless_after_move());
  EXPECT_EQ(*b, 99);
  EXPECT_EQ(allocs2, 1); // one new allocation using ta2
}

// ---------------------------------------------------------------------------
// Copy constructor — else branch
// (select_on_container_copy_construction returns a different allocator)
// ---------------------------------------------------------------------------

TEST(Allocator, CopyConstructorWithDivergingAllocatorAllocatesNewModel)
{
  // pmr::polymorphic_allocator::select_on_container_copy_construction() returns
  // an allocator using the *default* memory resource, which differs from the
  // custom pool, so the copy constructor takes the else branch.
  std::array<std::byte, 1024> buf;
  std::pmr::monotonic_buffer_resource pool(buf.data(), buf.size());
  std::pmr::polymorphic_allocator<int> pmr_alloc(&pool);

  xyz::pmr::copy_on_write<int> a(std::allocator_arg, pmr_alloc, 42);
  xyz::pmr::copy_on_write<int> b(a);

  EXPECT_FALSE(a.identical_to(b)); // fresh model allocated
  EXPECT_EQ(*a, 42);
  EXPECT_EQ(*b, 42);
}

// ---------------------------------------------------------------------------
// Copy assignment — else branch (non-POCCA, non-equal allocators)
// ---------------------------------------------------------------------------

TEST(Allocator, CopyAssignmentWithDifferentNonPropagatingAllocatorAllocatesNewModel)
{
  int allocs1 = 0, deallocs1 = 0;
  int allocs2 = 0, deallocs2 = 0;
  tracking_allocator<int> ta1(&allocs1, &deallocs1, 1);
  tracking_allocator<int> ta2(&allocs2, &deallocs2, 2);

  xyz::copy_on_write<int, tracking_allocator<int>> a(std::allocator_arg, ta1, 10);
  xyz::copy_on_write<int, tracking_allocator<int>> b(std::allocator_arg, ta2, 20);

  b = a;

  EXPECT_EQ(*b, 10);
  EXPECT_EQ(b.get_allocator(), ta2); // allocator not propagated (POCCA = false)
  EXPECT_EQ(allocs2, 2);             // one for construction, one for copy assignment
}
