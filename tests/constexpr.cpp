// SPDX-License-Identifier: BSL-1.0
#include <copy_on_write.hpp>
#include <gtest/gtest.h>
#include <vector>

#if (__cplusplus > 202302L || (defined(_MSVC_LANG) && _MSVC_LANG > 202302L)) &&                    \
  defined(__cpp_lib_constexpr_atomic) && __cpp_lib_constexpr_atomic >= 202411L
#  define TEST_CONSTEXPR constexpr
#else
#  define TEST_CONSTEXPR
#endif

struct hashable_value
{
  int value;
};

template <>
struct std::hash<hashable_value>
{
  constexpr std::size_t operator()(hashable_value const& v) const noexcept { return v.value; }
};

namespace {
TEST_CONSTEXPR bool ownership_and_mutation()
{
  xyz::copy_on_write<int> a(4);
  auto b = a;
  if (!a.identical_to(b) || a.use_count() != 2) {
    return false;
  }
  b.modify([](int& n) { ++n; });
  if (*a != 4 || *b != 5 || a.identical_to(b)) {
    return false;
  }
  auto c = a;
  c.modify([](int& n) { n += 2; }, [](int n) { return n + 2; });
  c.modify([](int& n) { ++n; }, [](int n) { return n + 100; });
  if (*c != 7 || *a != 4) {
    return false;
  }
  c = a;
  c = 9;
  if (*c != 9 || *a != 4) {
    return false;
  }
  auto const& alias = a;
  a = alias;
  b = std::move(c);
  auto empty = c;
  if (!empty.valueless_after_move() || empty != c || !(empty < a)) {
    return false;
  }
  swap(a, b);
  if (a != 9 || b != 4 || !(b < a) || !(a > 4)) {
    return false;
  }
  c = b;
  b = 8;
  if (*c != 4 || *b != 8) {
    return false;
  }
  c = empty;
  b = std::move(empty);
  return c.valueless_after_move() && b.valueless_after_move();
}

TEST_CONSTEXPR bool construction()
{
  std::allocator<int> const alloc;
  xyz::copy_on_write<int> a;
  xyz::copy_on_write<int> b(std::allocator_arg, alloc);
  xyz::copy_on_write<int> c(std::allocator_arg, alloc, 3);
  xyz::copy_on_write<int> d(std::in_place, 4);
  xyz::copy_on_write<int> e(std::allocator_arg, alloc, std::in_place, 5);
  xyz::copy_on_write<int> f(std::allocator_arg, alloc, c);
  xyz::copy_on_write<int> g(std::allocator_arg, alloc, std::move(f));
  xyz::copy_on_write<int> h(std::move(g));
  xyz::copy_on_write<std::vector<int>> v(std::in_place, {1, 2, 3});
  xyz::copy_on_write<std::vector<int>> w(std::allocator_arg, std::allocator<std::vector<int>>{},
                                         std::in_place, {4, 5});
  auto shared = v;
  shared.modify([](auto& vec) { vec.push_back(6); });
  return *a == 0 && *b == 0 && *c == 3 && *d == 4 && *e == 5 && c.identical_to(h) &&
    f.valueless_after_move() && g.valueless_after_move() && h.get_allocator() == alloc &&
    v->size() == 3 && shared->size() == 4 && w->back() == 5;
}

template <class T>
struct allocator
{
  using value_type = T;
  using is_always_equal = std::false_type;
  int id;
  int* outstanding;
  constexpr allocator(int i, int& count)
    : id(i)
    , outstanding(&count)
  {
  }
  template <class U>
  constexpr allocator(allocator<U> const& a) noexcept
    : id(a.id)
    , outstanding(a.outstanding)
  {
  }
  constexpr T* allocate(std::size_t n)
  {
    auto p = std::allocator<T>{}.allocate(n);
    ++*outstanding;
    return p;
  }
  constexpr void deallocate(T* p, std::size_t n) noexcept
  {
    --*outstanding;
    std::allocator<T>{}.deallocate(p, n);
  }
  constexpr allocator select_on_container_copy_construction() const noexcept
  {
    return allocator(id + 1, *outstanding);
  }
  constexpr bool operator==(allocator const& a) const noexcept { return id == a.id; }
};

struct payload
{
  int value;
  int* alive;
  constexpr payload(int n, int& count)
    : value(n)
    , alive(&count)
  {
    ++*alive;
  }
  constexpr payload(payload const& p)
    : payload(p.value, *p.alive)
  {
  }
  constexpr ~payload() { --*alive; }
};

TEST_CONSTEXPR bool allocation_and_lifetime()
{
  int outstanding = 0, alive = 0;
  {
    allocator<payload> a1(1, outstanding), a2(2, outstanding);
    using cow = xyz::copy_on_write<payload, allocator<payload>>;
    cow a(std::allocator_arg, a1, std::in_place, 7, alive);
    cow b(a); // allocator selection requires a deep copy
    if (a.identical_to(b) || alive != 2) {
      return false;
    }
    cow c(std::allocator_arg, a1, a);
    if (!a.identical_to(c) || alive != 2) {
      return false;
    }
    cow d(std::allocator_arg, a2, std::move(c));
    if (!c.valueless_after_move() || alive != 3) {
      return false;
    }
    d = a; // unequal non-propagating allocator
    b = std::move(a);
    if (!a.valueless_after_move() || b->value != 7 || d->value != 7) {
      return false;
    }
    auto shared = b;
    shared.modify([](payload& p) { ++p.value; });
    if (b->value != 7 || shared->value != 8) {
      return false;
    }
  }
  return outstanding == 0 && alive == 0;
}

TEST_CONSTEXPR bool hashing()
{
  xyz::copy_on_write<hashable_value> a(std::in_place, 42);
  auto b = a;
  using hash = std::hash<decltype(a)>;
  if (hash{}(a) != 42 || hash{}(b) != 42) {
    return false;
  }
  auto moved = std::move(a);
  auto empty = a;
  return hash{}(moved) == 42 && hash{}(a) == hash{}(empty);
}

#if (__cplusplus > 202302L || (defined(_MSVC_LANG) && _MSVC_LANG > 202302L)) &&                    \
  defined(__cpp_lib_constexpr_atomic) && __cpp_lib_constexpr_atomic >= 202411L
static_assert(ownership_and_mutation());
static_assert(construction());
static_assert(allocation_and_lifetime());
static_assert(hashing());
#endif

TEST(Constexpr, OwnershipAndMutation)
{
  EXPECT_TRUE(ownership_and_mutation());
}
TEST(Constexpr, Construction)
{
  EXPECT_TRUE(construction());
}
TEST(Constexpr, AllocationAndLifetime)
{
  EXPECT_TRUE(allocation_and_lifetime());
}
TEST(Constexpr, Hashing)
{
  EXPECT_TRUE(hashing());
}
} // namespace
#undef TEST_CONSTEXPR
