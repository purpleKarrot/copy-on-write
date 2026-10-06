// SPDX-License-Identifier: BSL-1.0

#include <copy_on_write.hpp>
#include <gtest/gtest.h>

#include <initializer_list>
#include <iterator>
#include <memory>
#include <memory_resource>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
    : alloc_count{ac}
    , dealloc_count{dc}
    , id{id_}
  {
  }

  template <typename U>
  tracking_allocator(tracking_allocator<U> const& o) noexcept
    : alloc_count{o.alloc_count}
    , dealloc_count{o.dealloc_count}
    , id{o.id}
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

// Pointer wrapper with no implicit conversion to a raw pointer.
template <typename T>
struct fancy_pointer
{
  using element_type = T;
  using value_type = std::remove_cv_t<T>;
  using difference_type = std::ptrdiff_t;
  using iterator_category = std::random_access_iterator_tag;
  using reference = std::add_lvalue_reference_t<T>;
  template <typename U> using rebind = fancy_pointer<U>;

  T* address = nullptr;
  constexpr fancy_pointer() noexcept = default;
  constexpr fancy_pointer(std::nullptr_t) noexcept {}
  explicit constexpr fancy_pointer(T* p) noexcept : address(p) {}
  template <typename U> requires std::is_convertible_v<U*, T*>
  constexpr fancy_pointer(fancy_pointer<U> p) noexcept : address(p.address) {}

  constexpr T* operator->() const noexcept { return address; }
  constexpr reference operator*() const noexcept { return *address; }
  explicit constexpr operator bool() const noexcept { return address != nullptr; }
  template <typename U = T> requires (!std::is_void_v<U>)
  static constexpr fancy_pointer pointer_to(U& value) noexcept
  {
    return fancy_pointer(std::addressof(value));
  }

  constexpr fancy_pointer& operator++() { ++address; return *this; }
  constexpr fancy_pointer operator++(int) { auto old = *this; ++*this; return old; }
  constexpr fancy_pointer& operator--() { --address; return *this; }
  constexpr fancy_pointer operator--(int) { auto old = *this; --*this; return old; }
  constexpr fancy_pointer& operator+=(difference_type n) { address += n; return *this; }
  constexpr fancy_pointer& operator-=(difference_type n) { address -= n; return *this; }
  constexpr reference operator[](difference_type n) const { return address[n]; }
  friend constexpr fancy_pointer operator+(fancy_pointer p, difference_type n) { return p += n; }
  friend constexpr fancy_pointer operator+(difference_type n, fancy_pointer p) { return p += n; }
  friend constexpr fancy_pointer operator-(fancy_pointer p, difference_type n) { return p -= n; }
  friend constexpr difference_type operator-(fancy_pointer a, fancy_pointer b)
  {
    return a.address - b.address;
  }
  constexpr bool operator==(fancy_pointer const&) const = default;
  constexpr auto operator<=>(fancy_pointer const&) const = default;
};

template <typename T>
struct fancy_allocator : tracking_allocator<T>
{
  using pointer = fancy_pointer<T>;
  template <typename U> struct rebind { using other = fancy_allocator<U>; };
  using tracking_allocator<T>::tracking_allocator;
  fancy_allocator() = default;
  template <typename U>
  fancy_allocator(fancy_allocator<U> const& other) noexcept : tracking_allocator<T>(other) {}

  pointer allocate(std::size_t n)
  {
    return pointer(tracking_allocator<T>::allocate(n));
  }
  void deallocate(pointer p, std::size_t n) noexcept
  {
    tracking_allocator<T>::deallocate(p.address, n);
  }
};

template <typename T>
struct brace_sensitive_allocator : tracking_allocator<T>
{
  template <typename U> struct rebind { using other = brace_sensitive_allocator<U>; };
  using tracking_allocator<T>::tracking_allocator;
  brace_sensitive_allocator() = default;
  brace_sensitive_allocator(std::initializer_list<brace_sensitive_allocator>) = delete;
  template <typename U>
  brace_sensitive_allocator(brace_sensitive_allocator<U> const& other) noexcept
    : tracking_allocator<T>(other)
  {
  }
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

template <typename T>
struct soccc_allocator : tracking_allocator<T>
{
  template <typename U>
  struct rebind
  {
    using other = soccc_allocator<U>;
  };

  using tracking_allocator<T>::tracking_allocator;

  template <typename U>
  soccc_allocator(soccc_allocator<U> const& o) noexcept
    : tracking_allocator<T>(o)
  {
  }

  soccc_allocator select_on_container_copy_construction() const noexcept
  {
    return soccc_allocator{};
  }
};

template <typename T, bool NothrowSelection>
struct selection_allocator : tracking_allocator<T>
{
  using is_always_equal = std::true_type;
  bool throw_on_selection = false;

  template <typename U>
  struct rebind
  {
    using other = selection_allocator<U, NothrowSelection>;
  };

  using tracking_allocator<T>::tracking_allocator;

  template <typename U>
  selection_allocator(selection_allocator<U, NothrowSelection> const& other) noexcept
    : tracking_allocator<T>(other)
    , throw_on_selection(other.throw_on_selection)
  {
  }

  selection_allocator select_on_container_copy_construction() const noexcept(NothrowSelection)
  {
    if constexpr (!NothrowSelection) {
      if (throw_on_selection) {
        throw std::runtime_error("allocator selection failed");
      }
    }
    return *this;
  }

  bool operator==(selection_allocator const&) const noexcept { return true; }
};

static_assert(std::is_nothrow_copy_constructible_v<xyz::copy_on_write<int>>);
static_assert(std::is_nothrow_copy_assignable_v<xyz::copy_on_write<int>>);
static_assert(
  std::is_nothrow_constructible_v<xyz::copy_on_write<int>, std::allocator_arg_t,
                                  std::allocator<int> const&, xyz::copy_on_write<int> const&>);
static_assert(!std::is_nothrow_copy_constructible_v<xyz::pmr::copy_on_write<int>>);
static_assert(!std::is_nothrow_copy_assignable_v<xyz::pmr::copy_on_write<int>>);
static_assert(!std::is_nothrow_constructible_v<xyz::pmr::copy_on_write<int>, std::allocator_arg_t,
                                               std::pmr::polymorphic_allocator<int> const&,
                                               xyz::pmr::copy_on_write<int> const&>);
static_assert(
  !std::is_nothrow_copy_constructible_v<xyz::copy_on_write<int, tracking_allocator<int>>>);
static_assert(!std::is_nothrow_copy_assignable_v<xyz::copy_on_write<int, tracking_allocator<int>>>);
static_assert(!std::is_nothrow_copy_constructible_v<xyz::copy_on_write<int, pocca_allocator<int>>>);
static_assert(std::is_nothrow_copy_assignable_v<xyz::copy_on_write<int, pocca_allocator<int>>>);
static_assert(!std::is_nothrow_copy_constructible_v<xyz::copy_on_write<int, soccc_allocator<int>>>);
static_assert(
  std::is_nothrow_copy_constructible_v<xyz::copy_on_write<int, selection_allocator<int, true>>>);
static_assert(
  !std::is_nothrow_copy_constructible_v<xyz::copy_on_write<int, selection_allocator<int, false>>>);
static_assert(
  std::is_nothrow_copy_assignable_v<xyz::copy_on_write<int, selection_allocator<int, false>>>);

struct move_observer
{
  std::string value;
  int* copies;
  int* moves;
  bool throw_on_copy;

  move_observer(std::string v, int& c, int& m, bool throws = false)
    : value{std::move(v)}
    , copies{&c}
    , moves{&m}
    , throw_on_copy{throws}
  {
  }

  move_observer(move_observer const& other)
    : value{other.value}
    , copies{other.copies}
    , moves{other.moves}
    , throw_on_copy{other.throw_on_copy}
  {
    ++*copies;
    if (throw_on_copy) {
      throw std::runtime_error("copy failed");
    }
  }

  move_observer(move_observer&& other) noexcept
    : value{std::exchange(other.value, "")}
    , copies{other.copies}
    , moves{other.moves}
    , throw_on_copy{other.throw_on_copy}
  {
    ++*moves;
  }
};

using observed_cow = xyz::copy_on_write<move_observer, tracking_allocator<move_observer>>;

} // namespace

TEST(Allocator, ThrowingSelectionPropagatesEvenWhenAllocatorsAreAlwaysEqual)
{
  for (bool valueless : {false, true}) {
    SCOPED_TRACE(valueless ? "valueless" : "live");
    using allocator = selection_allocator<int, false>;
    using cow = xyz::copy_on_write<int, allocator>;
    int allocs = 0, deallocs = 0;
    allocator a(&allocs, &deallocs, 1);
    a.throw_on_selection = true;
    {
      cow source(std::allocator_arg, a, 42);
      std::optional<cow> holder;
      if (valueless) {
        holder.emplace(std::move(source));
      }
      EXPECT_THROW((cow(source)), std::runtime_error);
      EXPECT_EQ(source.valueless_after_move(), valueless);
      EXPECT_EQ(valueless ? holder->use_count() : source.use_count(), 1);
      EXPECT_EQ(valueless ? **holder : *source, 42);
    }
    EXPECT_EQ(allocs, deallocs);
  }
}

TEST(Allocator, CopyAssignmentDoesNotSelectAllocator)
{
  using allocator = selection_allocator<int, false>;
  using cow = xyz::copy_on_write<int, allocator>;
  allocator a;
  a.throw_on_selection = true;
  cow source(std::allocator_arg, a, 42);
  cow target(std::allocator_arg, a, 0);
  target = source;
  EXPECT_TRUE(source.identical_to(target));
  EXPECT_EQ(*target, 42);
}

TEST(Allocator, DefaultAllocatorCopyDoesNotCopyThrowingPayload)
{
  struct ThrowOnCopy
  {
    ThrowOnCopy() = default;
    ThrowOnCopy(ThrowOnCopy const&) { throw std::runtime_error("payload copied"); }
  };
  using cow = xyz::copy_on_write<ThrowOnCopy>;
  static_assert(std::is_nothrow_copy_constructible_v<cow>);
  static_assert(std::is_nothrow_copy_assignable_v<cow>);
  cow source;
  cow copied(source);
  cow assigned;
  assigned = source;
  EXPECT_TRUE(source.identical_to(copied));
  EXPECT_TRUE(source.identical_to(assigned));
  EXPECT_EQ(source.use_count(), 3);
}

TEST(Allocator, CopyAssignmentWithIncompatibleAllocatorPropagatesPayloadException)
{
  int allocs = 0, deallocs = 0, copies = 0, moves = 0;
  tracking_allocator<move_observer> a(&allocs, &deallocs, 1), b(&allocs, &deallocs, 2);
  {
    observed_cow source(std::allocator_arg, a, std::in_place, "original", copies, moves, true);
    observed_cow target(std::allocator_arg, b, std::in_place, "destination", copies, moves);
    EXPECT_THROW(target = source, std::runtime_error);
    EXPECT_EQ(source->value, "original");
    EXPECT_EQ(target->value, "destination");
    EXPECT_EQ(target.get_allocator(), b);
    EXPECT_EQ(source.use_count(), 1);
    EXPECT_EQ(target.use_count(), 1);
  }
  EXPECT_EQ(allocs, deallocs);
}

TEST(Allocator, UnequalAllocatorMovePreservesSharedPayload)
{
  for (bool assignment : {false, true}) {
    SCOPED_TRACE(assignment ? "assignment" : "construction");
    int allocs = 0, deallocs = 0, copies = 0, moves = 0;
    tracking_allocator<move_observer> a(&allocs, &deallocs, 1), b(&allocs, &deallocs, 2);
    {
      observed_cow source(std::allocator_arg, a, std::in_place, "original", copies, moves);
      observed_cow peer(source);
      std::optional<observed_cow> target;
      if (assignment) {
        target.emplace(std::allocator_arg, b, std::in_place, "destination", copies, moves);
        *target = std::move(source);
      } else {
        target.emplace(std::allocator_arg, b, std::move(source));
      }
      EXPECT_EQ(peer->value, "original");
      EXPECT_EQ((*target)->value, "original");
      EXPECT_EQ(target->get_allocator(), b);
      EXPECT_TRUE(source.valueless_after_move());
      EXPECT_EQ(peer.use_count(), 1);
      EXPECT_EQ(copies, 1);
      EXPECT_EQ(moves, 0);
    }
    EXPECT_EQ(allocs, deallocs);
  }
}

TEST(Allocator, UnequalAllocatorMoveCopiesUniquePayloadThroughConstObserver)
{
  for (bool assignment : {false, true}) {
    SCOPED_TRACE(assignment ? "assignment" : "construction");
    int allocs = 0, deallocs = 0, copies = 0, moves = 0;
    tracking_allocator<move_observer> a(&allocs, &deallocs, 1), b(&allocs, &deallocs, 2);
    {
      observed_cow source(std::allocator_arg, a, std::in_place, "original", copies, moves);
      std::optional<observed_cow> target;
      if (assignment) {
        target.emplace(std::allocator_arg, b, std::in_place, "destination", copies, moves);
        *target = std::move(source);
      } else {
        target.emplace(std::allocator_arg, b, std::move(source));
      }
      EXPECT_EQ((*target)->value, "original");
      EXPECT_TRUE(source.valueless_after_move());
      EXPECT_EQ(copies, 1);
      EXPECT_EQ(moves, 0);
    }
    EXPECT_EQ(allocs, deallocs);
  }
}

TEST(Allocator, UnequalAllocatorMoveCopyFailurePreservesOwners)
{
  for (bool assignment : {false, true}) {
    SCOPED_TRACE(assignment ? "assignment" : "construction");
    int allocs = 0, deallocs = 0, copies = 0, moves = 0;
    tracking_allocator<move_observer> a(&allocs, &deallocs, 1), b(&allocs, &deallocs, 2);
    {
      observed_cow source(std::allocator_arg, a, std::in_place, "original", copies, moves, true);
      observed_cow peer(source);
      if (assignment) {
        observed_cow target(std::allocator_arg, b, std::in_place, "destination", copies, moves);
        EXPECT_THROW(target = std::move(source), std::runtime_error);
        EXPECT_EQ(target->value, "destination");
        EXPECT_EQ(target.get_allocator(), b);
      } else {
        EXPECT_THROW((observed_cow(std::allocator_arg, b, std::move(source))), std::runtime_error);
      }
      ASSERT_FALSE(source.valueless_after_move());
      EXPECT_TRUE(source.identical_to(peer));
      EXPECT_EQ(source->value, "original");
      EXPECT_EQ(peer->value, "original");
      EXPECT_EQ(copies, 1);
      EXPECT_EQ(moves, 0);
    }
    EXPECT_EQ(allocs, deallocs);
  }
}

// ---------------------------------------------------------------------------
// Exception safety
// ---------------------------------------------------------------------------

TEST(Allocator, ExceptionDeallocates)
{
  struct ThrowOnCopy
  {
    ThrowOnCopy() = default;
    ThrowOnCopy(ThrowOnCopy const&) { throw std::runtime_error("throws"); }
  };

  int allocs = 0, deallocs = 0;
  tracking_allocator<ThrowOnCopy> ta(&allocs, &deallocs, 1);

  EXPECT_THROW((xyz::copy_on_write<ThrowOnCopy, tracking_allocator<ThrowOnCopy>>(
                 std::allocator_arg, ta, ThrowOnCopy{})),
               std::runtime_error);
  EXPECT_EQ(allocs, 1);
  EXPECT_EQ(deallocs, 1);
}

TEST(Allocator, SharedActionFailureDeallocatesReplacementWithOriginalAllocator)
{
  int allocs = 0, deallocs = 0;
  tracking_allocator<int> alloc(&allocs, &deallocs, 1);
  {
    xyz::copy_on_write<int, tracking_allocator<int>> original(std::allocator_arg, alloc, 5);
    auto value = original;

    EXPECT_THROW(value.modify([](int& v) {
      v = 15;
      throw std::runtime_error("action failed");
    }),
                 std::runtime_error);

    EXPECT_EQ(allocs, 2);
    EXPECT_EQ(deallocs, 1);
    EXPECT_EQ(*value, 5);
    EXPECT_TRUE(value.identical_to(original));
    EXPECT_EQ(value.get_allocator(), alloc);
  }
  EXPECT_EQ(allocs, deallocs);
}

TEST(Allocator, SharedTransformationResultConstructionFailurePreservesOwnership)
{
  struct throwing_move
  {
    int value;
    explicit throwing_move(int v)
      : value(v)
    {
    }
    throwing_move(throwing_move const&) = default;
    throwing_move(throwing_move&&) { throw std::runtime_error("move failed"); }
  };
  int allocs = 0, deallocs = 0;
  tracking_allocator<throwing_move> alloc(&allocs, &deallocs, 1);
  {
    xyz::copy_on_write<throwing_move, tracking_allocator<throwing_move>> original(
      std::allocator_arg, alloc, std::in_place, 5);
    auto value = original;
    auto address = &*value;
    bool action_called = false;

    EXPECT_THROW(value.modify([&](throwing_move&) { action_called = true; },
                              [](throwing_move const& v) { return throwing_move(v.value + 1); }),
                 std::runtime_error);

    EXPECT_FALSE(action_called);
    EXPECT_EQ(value->value, 5);
    EXPECT_EQ(&*value, address);
    EXPECT_TRUE(value.identical_to(original));
    EXPECT_EQ(value.use_count(), 2);
    EXPECT_EQ(value.get_allocator(), alloc);
    EXPECT_EQ(allocs, 2);
    EXPECT_EQ(deallocs, 1);
  }
  EXPECT_EQ(allocs, deallocs);
}

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
  int allocs = 0, deallocs = 0;
  soccc_allocator<int> da(&allocs, &deallocs, 1);

  xyz::copy_on_write<int, soccc_allocator<int>> a(std::allocator_arg, da, 42);
  xyz::copy_on_write<int, soccc_allocator<int>> b(a);

  EXPECT_FALSE(a.identical_to(b));           // fresh model allocated
  EXPECT_NE(a.get_allocator(), b.get_allocator()); // allocators diverged
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

TEST(Allocator, ValueAssignmentPreservesPropagatingAllocator)
{
  int allocs = 0, deallocs = 0;
  pocma_allocator<int> alloc(&allocs, &deallocs, 7);
  {
    xyz::copy_on_write<int, pocma_allocator<int>> original(std::allocator_arg, alloc, 1);
    auto value = original;
    value = 42;
    EXPECT_EQ(value.get_allocator(), alloc);
    EXPECT_EQ(*value, 42);
    EXPECT_EQ(*original, 1);
    EXPECT_EQ(allocs, 2);
    EXPECT_EQ(deallocs, 0);

    auto moved = std::move(value);
    value = 99;
    EXPECT_EQ(value.get_allocator(), alloc);
    EXPECT_EQ(*value, 99);
    EXPECT_EQ(*moved, 42);
    EXPECT_EQ(allocs, 3);
  }
  EXPECT_EQ(deallocs, allocs);
}

TEST(Allocator, ValueConstructionFailurePreservesOwnershipAndAllocator)
{
  struct payload
  {
    int value;
    explicit payload(int n) : value(n)
    {
      if (n < 0) {
        throw std::runtime_error("construction failed");
      }
    }
    payload& operator=(int n)
    {
      value = n;
      return *this;
    }
  };

  int allocs = 0, deallocs = 0;
  tracking_allocator<payload> alloc(&allocs, &deallocs, 7);
  {
    xyz::copy_on_write<payload, tracking_allocator<payload>> original(
      std::allocator_arg, alloc, std::in_place, 1);
    auto value = original;
    EXPECT_THROW(value = -1, std::runtime_error);
    EXPECT_TRUE(value.identical_to(original));
    EXPECT_EQ(value->value, 1);
    EXPECT_EQ(value.get_allocator(), alloc);
    EXPECT_EQ(allocs, 2);
    EXPECT_EQ(deallocs, 1);

    auto moved = std::move(value);
    EXPECT_THROW(value = -1, std::runtime_error);
    EXPECT_TRUE(value.valueless_after_move());
    EXPECT_EQ(value.get_allocator(), alloc);
    EXPECT_EQ(allocs, 3);
    EXPECT_EQ(deallocs, 2);
  }
  EXPECT_EQ(deallocs, allocs);
}

TEST(Allocator, AlwaysEqualAllocatorMovesNoncopyablePayloadWithoutCopying)
{
  using payload = std::unique_ptr<int>;
  using cow = xyz::copy_on_write<payload>;
  cow source(std::in_place, std::make_unique<int>(42));
  auto address = &*source;
  cow target(std::allocator_arg, std::allocator<payload>{}, std::move(source));
  EXPECT_TRUE(source.valueless_after_move());
  EXPECT_EQ(&*target, address);
  EXPECT_EQ(**target, 42);

  source = std::move(target);
  EXPECT_TRUE(target.valueless_after_move());
  EXPECT_EQ(&*source, address);
  EXPECT_EQ(**source, 42);

  cow empty(std::allocator_arg, std::allocator<payload>{}, std::move(target));
  EXPECT_TRUE(empty.valueless_after_move());
  source = std::move(empty);
  EXPECT_TRUE(source.valueless_after_move());
}

TEST(Allocator, PropagatingMoveTransfersNoncopyablePayloadBetweenUnequalAllocators)
{
  using payload = std::unique_ptr<int>;
  using allocator = pocma_allocator<payload>;
  using cow = xyz::copy_on_write<payload, allocator>;
  int allocs1 = 0, deallocs1 = 0, allocs2 = 0, deallocs2 = 0;
  allocator a(&allocs1, &deallocs1, 1), b(&allocs2, &deallocs2, 2);
  {
    cow source(std::allocator_arg, a, std::in_place, std::make_unique<int>(42));
    cow target(std::allocator_arg, b, std::in_place, std::make_unique<int>(7));
    auto address = &*source;
    target = std::move(source);
    EXPECT_TRUE(source.valueless_after_move());
    EXPECT_EQ(&*target, address);
    EXPECT_EQ(**target, 42);
    EXPECT_EQ(target.get_allocator(), a);
    EXPECT_EQ(allocs1, 1);
    EXPECT_EQ(allocs2, 1);
    EXPECT_EQ(deallocs1, 0);
    EXPECT_EQ(deallocs2, 1);
  }
  EXPECT_EQ(allocs1, deallocs1);
  EXPECT_EQ(allocs2, deallocs2);
}

TEST(Allocator, UnequalAllocatorMoveConstructionUsesConstLvalueCopy)
{
  struct payload
  {
    int value;
    explicit payload(int n) : value(n) {}
    payload(payload const&) = default;
    payload(payload const&&) = delete;
  };
  using allocator = tracking_allocator<payload>;
  using cow = xyz::copy_on_write<payload, allocator>;
  int allocs1 = 0, deallocs1 = 0, allocs2 = 0, deallocs2 = 0;
  allocator a(&allocs1, &deallocs1, 1), b(&allocs2, &deallocs2, 2);
  {
    cow source(std::allocator_arg, a, std::in_place, 42);
    auto peer = source;
    cow target(std::allocator_arg, b, std::move(source));
    EXPECT_TRUE(source.valueless_after_move());
    EXPECT_EQ(target->value, 42);
    EXPECT_EQ(peer->value, 42);
    EXPECT_FALSE(target.identical_to(peer));
    EXPECT_EQ(target.get_allocator(), b);
    EXPECT_EQ(peer.use_count(), 1);
    EXPECT_EQ(allocs1, 1);
    EXPECT_EQ(allocs2, 1);
  }
  EXPECT_EQ(allocs1, deallocs1);
  EXPECT_EQ(allocs2, deallocs2);
}

TEST(Allocator, FancyPointersSupportOwnershipAndObservers)
{
  using cow = xyz::copy_on_write<int, fancy_allocator<int>>;
  static_assert(std::same_as<cow::const_pointer, fancy_pointer<const int>>);
  static_assert(!std::is_convertible_v<fancy_pointer<int>, int*>);
  int allocs = 0, deallocs = 0;
  fancy_allocator<int> alloc(&allocs, &deallocs, 7);
  {
    cow original(std::allocator_arg, alloc, 1);
    auto value = original;
    EXPECT_TRUE(value.identical_to(original));
    EXPECT_EQ(value.use_count(), 2);
    EXPECT_EQ(*value.operator->(), 1);
    value.modify([](int& n) { n = 2; });
    EXPECT_EQ(*original, 1);
    EXPECT_EQ(*value, 2);
    EXPECT_FALSE(value.identical_to(original));
    value = 3;
    auto moved = std::move(value);
    EXPECT_TRUE(value.valueless_after_move());
    original.swap(moved);
    EXPECT_EQ(*original, 3);
    EXPECT_EQ(*moved, 1);
    value = 4;
    EXPECT_EQ(*value, 4);
    EXPECT_EQ(allocs, 3);
  }
  EXPECT_EQ(allocs, deallocs);
}

TEST(Allocator, FancyPointersDeallocateFailedReplacement)
{
  int allocs = 0, deallocs = 0;
  fancy_allocator<int> alloc(&allocs, &deallocs, 7);
  {
    xyz::copy_on_write<int, fancy_allocator<int>> original(std::allocator_arg, alloc, 1);
    auto value = original;
    EXPECT_THROW(value.modify([](int& n) {
      n = 2;
      throw std::runtime_error("action failed");
    }), std::runtime_error);
    EXPECT_TRUE(value.identical_to(original));
    EXPECT_EQ(*value, 1);
    EXPECT_EQ(allocs, 2);
    EXPECT_EQ(deallocs, 1);
  }
  EXPECT_EQ(allocs, deallocs);
}

TEST(Allocator, AllocatorInitializationDoesNotSelectInitializerListConstructor)
{
  using payload = std::vector<int>;
  using allocator = brace_sensitive_allocator<payload>;
  using cow = xyz::copy_on_write<payload, allocator>;
  static_assert(std::is_copy_constructible_v<allocator>);
  int allocs = 0, deallocs = 0;
  allocator alloc(&allocs, &deallocs, 7);
  {
    cow empty(std::allocator_arg, alloc);
    cow value(std::allocator_arg, alloc, payload{1, 2});
    cow in_place(std::allocator_arg, alloc, std::in_place, 3u, 7);
    cow list(std::allocator_arg, alloc, std::in_place, {4, 5});
    cow copied(value);
    cow moved(std::move(copied));
    cow allocated_copy(std::allocator_arg, alloc, value);
    cow allocated_move(std::allocator_arg, alloc, std::move(allocated_copy));

    EXPECT_TRUE(empty->empty());
    EXPECT_EQ(*value, (payload{1, 2}));
    EXPECT_EQ(*in_place, (payload{7, 7, 7}));
    EXPECT_EQ(*list, (payload{4, 5}));
    EXPECT_TRUE(moved.identical_to(value));
    EXPECT_TRUE(allocated_move.identical_to(value));
    EXPECT_TRUE(copied.valueless_after_move());
    EXPECT_TRUE(allocated_copy.valueless_after_move());
    EXPECT_EQ(moved.get_allocator(), alloc);
    EXPECT_EQ(allocated_move.get_allocator(), alloc);
    EXPECT_EQ(allocs, 4);
  }
  EXPECT_EQ(allocs, deallocs);
}
