// SPDX-License-Identifier: BSL-1.0

/// @file copy_on_write.hpp
/// @brief Provides the `xyz::copy_on_write` class template — a vocabulary type
///        for lazily-copied, reference-counted values with full value semantics.

#ifndef XYZ_COPY_ON_WRITE_HPP
#define XYZ_COPY_ON_WRITE_HPP

#include <atomic>
#include <cassert>
#include <compare>
#include <cstddef>
#include <functional>
#include <memory>
#include <memory_resource>
#include <type_traits>
#include <utility>

/// @namespace xyz
/// @brief Top-level namespace for the `copy_on_write` library.
namespace xyz {

/// @cond FORWARD_DECLARATIONS
template <typename T, typename Allocator = std::allocator<T>>
class copy_on_write;
/// @endcond

namespace detail {

template <typename F, typename T>
concept action = std::invocable<F, T&> && std::same_as<std::invoke_result_t<F, T&>, void>;

template <typename F, typename T>
concept transformation =
  std::invocable<F, T const&> && std::same_as<std::invoke_result_t<F, T const&>, T>;

template <typename T>
concept hashable = requires(T t) { std::hash<T>{}(t); };

template <typename>
inline constexpr bool is_copy_on_write_v = false;

template <typename T, typename A>
inline constexpr bool is_copy_on_write_v<copy_on_write<T, A>> = true;

template <typename>
inline constexpr bool is_in_place_type_v = false;

template <typename T>
inline constexpr bool is_in_place_type_v<std::in_place_type_t<T>> = true;

template <typename T, typename U>
  requires std::three_way_comparable_with<T, U>
constexpr auto synth_three_way(T const& t, U const& u) -> std::compare_three_way_result_t<T, U>
{
  return t <=> u;
}

template <typename T, typename U>
  requires(requires(T const& t, U const& u) {
    { t < u } -> std::convertible_to<bool>;
    { u < t } -> std::convertible_to<bool>;
  } && !std::three_way_comparable_with<T, U>)
constexpr auto synth_three_way(T const& t, U const& u) -> std::weak_ordering
{
  if (t < u) {
    return std::weak_ordering::less;
  }
  if (u < t) {
    return std::weak_ordering::greater;
  }
  return std::weak_ordering::equivalent;
}

template <typename T, typename U = T>
using synth_three_way_result =
  decltype(synth_three_way(std::declval<T const&>(), std::declval<U const&>()));

} // namespace detail

/// @brief A class template that provides lazily-copied, reference-counted
///        ownership of a value of type @p T with full value semantics.
///
/// `copy_on_write<T>` manages a dynamically-allocated owned object of type @p T
/// with shared, reference-counted ownership. Copies of a `copy_on_write<T>`
/// share ownership of the same underlying object; the object is only copied when
/// a modification is requested and the reference count exceeds one. This gives
/// efficient value semantics: copying is O(1) regardless of the size or
/// complexity of @p T, and mutation is deferred until necessary.
///
/// Access to the owned object through `operator*` and `operator->` is always
/// const-qualified. Mutation of the owned object is performed exclusively
/// through the `modify` member function, which copies the shared data when
/// necessary and provides a structured interface for efficient in-place or
/// constructive modification.
///
/// After a move operation the source object enters a *valueless* state
/// (see `valueless_after_move()`). Most member functions require the object to
/// be non-valueless; this is asserted in debug builds.
///
/// @tparam T         The type of the managed object. Must be a non-array,
///                   non-cv-qualified object type that is not `std::in_place_t`
///                   or a specialisation of `std::in_place_type_t`.
/// @tparam Allocator An allocator whose `value_type` is @p T. Defaults to
///                   `std::allocator<T>`.
template <typename T, typename Allocator>
class copy_on_write
{
  using alloc_traits = std::allocator_traits<Allocator>;

  static_assert(std::is_object_v<T>, "T must be an object type");
  static_assert(!std::is_array_v<T>, "T must not be an array type");
  static_assert(!std::is_same_v<T, std::in_place_t>, "T must not be std::in_place_t");
  static_assert(!detail::is_in_place_type_v<T>,
                "T must not be a specialization of std::in_place_type_t");
  static_assert(!std::is_const_v<T> && !std::is_volatile_v<T>, "T must not be cv-qualified");
  static_assert(std::is_same_v<T, typename alloc_traits::value_type>,
                "Allocator::value_type must be T");

public:
  /// The type of the managed object.
  using value_type = T;
  /// The allocator type used to allocate and construct the managed object.
  using allocator_type = Allocator;
  /// A const pointer to the managed object, as provided by the allocator.
  using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;

  //
  // Member functions
  //

  /// @brief Default-constructs the managed object using a default-constructed allocator.
  ///
  /// @pre `T` must be default-constructible.
  /// @post `!valueless_after_move()` and `use_count() == 1`.
  explicit copy_on_write()
    requires std::default_initializable<Allocator>
    : _alloc{}
    , _self{_make_model(_alloc)}
  {
    static_assert(std::is_default_constructible_v<T>);
  }

  /// @brief Constructs the managed object from a forwarded value using a
  ///        default-constructed allocator.
  ///
  /// @tparam U    A type from which @p T is constructible. Must not be
  ///              `copy_on_write` or `std::in_place_t`.
  /// @param  x    The value forwarded to the constructor of @p T.
  /// @post `!valueless_after_move()` and `use_count() == 1`.
  template <typename U = T>
    requires(!std::same_as<std::remove_cvref_t<U>, copy_on_write> &&
             !std::same_as<std::remove_cvref_t<U>, std::in_place_t> &&
             std::constructible_from<T, U> && std::default_initializable<Allocator>)
  explicit copy_on_write(U&& x)
    : _alloc{}
    , _self{_make_model(_alloc, std::forward<U>(x))}
  {
  }

  /// @brief Constructs the managed object in-place from the given arguments,
  ///        using a default-constructed allocator.
  ///
  /// @tparam Us   Types of arguments forwarded to the constructor of @p T.
  /// @param  us   Arguments forwarded to the constructor of @p T.
  /// @post `!valueless_after_move()` and `use_count() == 1`.
  template <typename... Us>
    requires(std::constructible_from<T, Us...> && std::default_initializable<Allocator>)
  explicit copy_on_write(std::in_place_t, Us&&... us)
    : _alloc{}
    , _self{_make_model(_alloc, std::forward<Us>(us)...)}
  {
  }

  /// @brief Constructs the managed object in-place from an initializer list and
  ///        additional arguments, using a default-constructed allocator.
  ///
  /// @tparam I    The element type of the initializer list.
  /// @tparam Us   Types of additional arguments forwarded to the constructor of @p T.
  /// @param  ilist Initializer list forwarded to the constructor of @p T.
  /// @param  us    Additional arguments forwarded to the constructor of @p T.
  /// @post `!valueless_after_move()` and `use_count() == 1`.
  template <typename I, typename... Us>
    requires(std::constructible_from<T, std::initializer_list<I>&, Us...> &&
             std::default_initializable<Allocator>)
  explicit copy_on_write(std::in_place_t, std::initializer_list<I> ilist, Us&&... us)
    : _alloc{}
    , _self{_make_model(_alloc, ilist, std::forward<Us>(us)...)}
  {
  }

  /// @brief Default-constructs the managed object using the provided allocator.
  ///
  /// @param a  The allocator to use.
  /// @pre `T` must be default-constructible.
  /// @post `!valueless_after_move()` and `use_count() == 1`.
  explicit copy_on_write(std::allocator_arg_t, Allocator const& a)
    : _alloc{a}
    , _self{_make_model(_alloc)}
  {
    static_assert(std::is_default_constructible_v<T>);
  }

  /// @brief Constructs the managed object from a forwarded value using the
  ///        provided allocator.
  ///
  /// @tparam U    A type from which @p T is constructible. Must not be
  ///              `copy_on_write` or `std::in_place_t`.
  /// @param  a    The allocator to use.
  /// @param  u    The value forwarded to the constructor of @p T.
  /// @post `!valueless_after_move()` and `use_count() == 1`.
  template <typename U = T>
    requires(!std::same_as<std::remove_cvref_t<U>, copy_on_write> &&
             !std::same_as<std::remove_cvref_t<U>, std::in_place_t> &&
             std::constructible_from<T, U>)
  explicit copy_on_write(std::allocator_arg_t, Allocator const& a, U&& u)
    : _alloc{a}
    , _self{_make_model(_alloc, std::forward<U>(u))}
  {
  }

  /// @brief Constructs the managed object in-place from the given arguments
  ///        using the provided allocator.
  ///
  /// @tparam Us   Types of arguments forwarded to the constructor of @p T.
  /// @param  a    The allocator to use.
  /// @param  us   Arguments forwarded to the constructor of @p T.
  /// @post `!valueless_after_move()` and `use_count() == 1`.
  template <typename... Us>
    requires std::constructible_from<T, Us...>
  explicit copy_on_write(std::allocator_arg_t, Allocator const& a, std::in_place_t, Us&&... us)
    : _alloc{a}
    , _self{_make_model(_alloc, std::forward<Us>(us)...)}
  {
  }

  /// @brief Constructs the managed object in-place from an initializer list and
  ///        additional arguments using the provided allocator.
  ///
  /// @tparam I    The element type of the initializer list.
  /// @tparam Us   Types of additional arguments forwarded to the constructor of @p T.
  /// @param  a     The allocator to use.
  /// @param  ilist Initializer list forwarded to the constructor of @p T.
  /// @param  us    Additional arguments forwarded to the constructor of @p T.
  /// @post `!valueless_after_move()` and `use_count() == 1`.
  template <typename I, typename... Us>
    requires std::constructible_from<T, std::initializer_list<I>&, Us...>
  explicit copy_on_write(std::allocator_arg_t, Allocator const& a, std::in_place_t,
                         std::initializer_list<I> ilist, Us&&... us)
    : _alloc{a}
    , _self{_make_model(_alloc, ilist, std::forward<Us>(us)...)}
  {
  }

  /// @brief Copy-constructs from @p other using the provided allocator.
  ///
  /// If @p a equals `other`'s allocator the underlying object is shared (O(1)).
  /// Otherwise a deep copy of the managed object is made.
  /// If @p other is valueless, `*this` will also be valueless after construction.
  ///
  /// @param a     The allocator to use.
  /// @param other The source object.
  /// @pre `T` must be copy-constructible.
  explicit copy_on_write(std::allocator_arg_t, Allocator const& a, copy_on_write const& other)
    : _alloc{a}
    , _self{nullptr}
  {
    static_assert(std::is_copy_constructible_v<T>);
    if (other.valueless_after_move()) {
      return;
    }
    if (a == other._alloc) {
      _self = other._self;
      _self->count.fetch_add(1, std::memory_order_relaxed);
    } else {
      _self = _make_model(_alloc, *other);
    }
  }

  /// @brief Move-constructs from @p other using the provided allocator.
  ///
  /// If the allocators are equal the ownership is transferred without
  /// allocating. Otherwise a new object is constructed by moving from `*other`.
  /// If @p other is valueless, `*this` will also be valueless after construction.
  ///
  /// @param a     The allocator to use.
  /// @param other The source object. Left in a valueless state if ownership is
  ///              transferred.
  explicit copy_on_write(std::allocator_arg_t, Allocator const& a,
                         copy_on_write&& other) noexcept(alloc_traits::is_always_equal::value)
    : _alloc{a}
    , _self{nullptr}
  {
    if (other.valueless_after_move()) {
      return;
    }

    if constexpr (alloc_traits::is_always_equal::value) {
      _self = std::exchange(other._self, nullptr);
    } else if (a == other._alloc) {
      _self = std::exchange(other._self, nullptr);
    } else {
      _self = _make_model(_alloc, std::move(*other));
      other._reset(nullptr);
    }
  }

  /// @brief Copy constructor.
  ///
  /// Shares ownership of the underlying object with @p x (O(1)). If the
  /// propagated allocator differs from @p x's allocator, a deep copy is made
  /// instead. If @p x is valueless, `*this` will also be valueless.
  ///
  /// @param x The source object.
  /// @pre `T` must be copy-constructible.
  copy_on_write(copy_on_write const& x)
    : _alloc{alloc_traits::select_on_container_copy_construction(x._alloc)}
    , _self{nullptr}
  {
    static_assert(std::is_copy_constructible_v<T>);

    if (x.valueless_after_move()) {
      return;
    }

    if constexpr (alloc_traits::is_always_equal::value) {
      _self = x._self;
      _self->count.fetch_add(1, std::memory_order_relaxed);
    } else if (_alloc == x._alloc) {
      _self = x._self;
      _self->count.fetch_add(1, std::memory_order_relaxed);
    } else {
      _self = _make_model(_alloc, *x);
    }
  }

  /// @brief Move constructor.
  ///
  /// Transfers ownership from @p x to `*this`. @p x is left in a valueless
  /// state. This constructor never throws.
  ///
  /// @param x The source object. Left in a valueless state after the call.
  copy_on_write(copy_on_write&& x) noexcept
    : _alloc{x._alloc}
    , _self{std::exchange(x._self, nullptr)}
  {
  }

  /// @brief Destructor.
  ///
  /// Decrements the reference count. If the count reaches zero, destroys and
  /// deallocates the managed object.
  ~copy_on_write()
  {
    assert(valueless_after_move() || _self->count > 0);
    if (_self != nullptr && _self->count.fetch_sub(1, std::memory_order_release) == 1) {
      std::atomic_thread_fence(std::memory_order_acquire);
      _destroy_model(_alloc, _self);
    }
  }

  /// @brief Copy assignment operator.
  ///
  /// Shares ownership of the underlying object managed by @p x. If the
  /// allocators are incompatible a deep copy is made. Self-assignment is a
  /// no-op. If @p x is valueless, `*this` becomes valueless after the call.
  ///
  /// @param x The source object.
  /// @pre `T` must be copy-constructible.
  /// @return A reference to `*this`.
  auto operator=(copy_on_write const& x) -> copy_on_write&
  {
    static_assert(std::is_copy_constructible_v<T>);

    if (std::addressof(x) == this) {
      return *this;
    }

    constexpr bool pocca = alloc_traits::propagate_on_container_copy_assignment::value;

    if (x.valueless_after_move()) {
      _reset(nullptr);
    } else if constexpr (pocca || alloc_traits::is_always_equal::value) {
      x._self->count.fetch_add(1, std::memory_order_relaxed);
      _reset(x._self);
    } else if (_alloc == x._alloc) {
      x._self->count.fetch_add(1, std::memory_order_relaxed);
      _reset(x._self);
    } else {
      _reset(_make_model(_alloc, *x));
    }

    if constexpr (pocca) {
      _alloc = x._alloc;
    }

    return *this;
  }

  /// @brief Move assignment operator.
  ///
  /// Transfers ownership from @p x to `*this`. Self-assignment is a no-op.
  /// If @p x is valueless, `*this` becomes valueless after the call.
  ///
  /// @param x The source object. Left in a valueless state if ownership is
  ///          transferred.
  /// @return A reference to `*this`.
  auto operator=(copy_on_write&& x) noexcept(
    alloc_traits::propagate_on_container_move_assignment::value ||
    alloc_traits::is_always_equal::value) -> copy_on_write&
  {
    if (std::addressof(x) == this) {
      return *this;
    }

    constexpr bool pocma = alloc_traits::propagate_on_container_move_assignment::value;

    if (x.valueless_after_move()) {
      _reset(nullptr);
    } else if (pocma || _alloc == x._alloc) {
      _reset(std::exchange(x._self, nullptr));
    } else {
      _reset(_make_model(_alloc, std::move(x._self->value)));
      x._reset(nullptr);
    }

    if constexpr (pocma) {
      _alloc = x._alloc;
    }

    return *this;
  }

  /// @brief Assigns a value to the managed object.
  ///
  /// If the reference count is one the managed object is assigned in-place.
  /// Otherwise a new object is constructed from @p x and ownership is replaced.
  ///
  /// @tparam U   A type from which @p T is constructible and assignable. Must
  ///             not be `copy_on_write`.
  /// @param  x   The value to assign.
  /// @return A reference to `*this`.
  template <typename U = T>
    requires(!std::same_as<std::remove_cvref_t<U>, copy_on_write> &&
             std::constructible_from<T, U> && std::assignable_from<T&, U>)
  auto operator=(U&& x) -> copy_on_write&
  {
    if (_self != nullptr && use_count() == 1) {
      _self->value = std::forward<U>(x);
      return *this;
    }

    return *this = copy_on_write(std::forward<U>(x));
  }

  //
  // Observers
  //

  /// @brief Returns a const reference to the managed object.
  ///
  /// @pre `!valueless_after_move()`.
  /// @return A const reference to the managed object.
  auto operator*() const noexcept -> value_type const&
  {
    assert(!valueless_after_move());
    return _self->value;
  }

  /// @brief Returns a const pointer to the managed object.
  ///
  /// @pre `!valueless_after_move()`.
  /// @return A const pointer to the managed object.
  auto operator->() const noexcept -> const_pointer
  {
    assert(!valueless_after_move());
    return std::pointer_traits<const_pointer>::pointer_to(_self->value);
  }

  /// @brief Checks whether the object is in a valueless state.
  ///
  /// An object enters the valueless state after being moved from. Most member
  /// functions have a precondition of `!valueless_after_move()`.
  ///
  /// @return `true` if the object is valueless, `false` otherwise.
  [[nodiscard]] auto valueless_after_move() const noexcept -> bool
  {
    return _self == nullptr;
  }

  /// @brief Returns the allocator associated with this object.
  ///
  /// Useful for passing to allocator-aware constructors when creating related
  /// objects that should use the same memory resource.
  ///
  /// @return A copy of the allocator.
  [[nodiscard]] auto get_allocator() const noexcept -> allocator_type
  {
    return _alloc;
  }

  /// @brief Returns the number of `copy_on_write` objects sharing the
  ///        managed object.
  ///
  /// @pre `!valueless_after_move()`.
  /// @return The current reference count. Returns `1` when the object is
  ///         uniquely owned.
  [[nodiscard]] auto use_count() const noexcept -> long
  {
    assert(!valueless_after_move());
    return _self->count.load(std::memory_order_acquire);
  }

  /// @brief Checks whether `*this` and @p x share the same managed object.
  ///
  /// Two `copy_on_write` objects are *identical* if and only if they point to
  /// the same internal storage, i.e. no copy has occurred between them.
  ///
  /// @param x The other object to compare against.
  /// @pre Neither `*this` nor @p x is valueless.
  /// @return `true` if both objects share the same managed object.
  [[nodiscard]] auto identical_to(copy_on_write const& x) const noexcept -> bool
  {
    assert(!valueless_after_move() && !x.valueless_after_move());
    return _self == x._self;
  }

  //
  // Modifiers
  //

  /// @brief Mutates the managed object in-place using the provided action.
  ///
  /// If the reference count is greater than one, the managed object is first
  /// copied so that `*this` becomes the sole owner; then @p action is invoked
  /// with a non-const reference to the (now exclusively owned) object.
  ///
  /// @tparam Action An invocable of the form `void(T&)`.
  /// @param  action The action to invoke on the managed object.
  /// @pre `!valueless_after_move()`.
  template <detail::action<T> Action>
  void modify(Action&& action)
  {
    if (use_count() > 1) {
      auto* p = _make_model(_alloc, std::as_const(_self->value));
      _self->count.fetch_sub(1, std::memory_order_release);
      _self = p;
    }

    std::forward<Action>(action)(_self->value);
  }

  /// @brief Mutates the managed object using either an in-place action or a
  ///        constructive transformation, depending on whether the object is
  ///        exclusively owned.
  ///
  /// If the reference count is one, @p action is applied directly to the
  /// managed object. Otherwise @p transform is called with a const reference to
  /// the current value and the result is used to construct a new managed object,
  /// making `*this` the sole owner of it.
  ///
  /// This overload is useful when an efficient in-place mutation (`action`) and
  /// a constructive alternative (`transform`) are both available.
  ///
  /// @tparam Action    An invocable of the form `void(T&)`.
  /// @tparam Transform An invocable of the form `T(T const&)`.
  /// @param  action    The action applied when the object is exclusively owned.
  /// @param  transform The transformation applied when the object is shared.
  /// @pre `!valueless_after_move()`.
  template <detail::action<T> Action, detail::transformation<T> Transform>
  void modify(Action&& action, Transform&& transform)
  {
    if (use_count() > 1) {
      auto* p =
        _make_model(_alloc, std::forward<Transform>(transform)(std::as_const(_self->value)));
      _self->count.fetch_sub(1, std::memory_order_release);
      _self = p;
    } else {
      std::forward<Action>(action)(_self->value);
    }
  }

  /// @brief Mutates the managed object using a constructive transformation.
  ///
  /// @p transform is called with a const reference to the current value and
  /// returns a new value of type @p T. If the object is exclusively owned the
  /// result is assigned in-place; otherwise a new managed object is allocated
  /// and `*this` becomes the sole owner.
  ///
  /// @tparam Transform An invocable of the form `T(T const&)`.
  /// @param  transform The transformation to apply.
  /// @pre `!valueless_after_move()`.
  template <detail::transformation<T> Transform>
  void modify(Transform&& transform)
  {
    if (use_count() > 1) {
      auto* p =
        _make_model(_alloc, std::forward<Transform>(transform)(std::as_const(_self->value)));
      _self->count.fetch_sub(1, std::memory_order_release);
      _self = p;
    } else {
      _self->value = std::forward<Transform>(transform)(std::as_const(_self->value));
    }
  }

  /// @brief Swaps the managed objects (and optionally the allocators) of
  ///        `*this` and @p other.
  ///
  /// @param other The object to swap with.
  /// @pre `alloc_traits::propagate_on_container_swap::value` or the allocators
  ///      compare equal.
  void swap(copy_on_write& other) noexcept(alloc_traits::propagate_on_container_swap::value ||
                                           alloc_traits::is_always_equal::value)
  {
    assert(alloc_traits::propagate_on_container_swap::value || _alloc == other._alloc);

    using std::swap;
    swap(_self, other._self);

    if constexpr (alloc_traits::propagate_on_container_swap::value) {
      swap(_alloc, other._alloc);
    }
  }

private:
  struct model
  {
    std::atomic<long> count;
    value_type value;
  };

  using model_alloc_t = typename alloc_traits::template rebind_alloc<model>;

  template <typename... Args>
  static auto _make_model(Allocator& a, Args&&... args)
  {
    auto ma = model_alloc_t{a};
    auto p = std::allocator_traits<model_alloc_t>::allocate(ma, 1);
    ::new (std::addressof(p->count)) std::atomic<long>{1};
    try {
      alloc_traits::construct(a, std::addressof(p->value), std::forward<Args>(args)...);
    } catch (...) {
      std::allocator_traits<model_alloc_t>::deallocate(ma, p, 1);
      throw;
    }
    return p;
  }

  static void _destroy_model(Allocator& a, model* p)
  {
    auto ma = model_alloc_t{a};
    alloc_traits::destroy(a, std::addressof(p->value));
    std::allocator_traits<model_alloc_t>::deallocate(ma, p, 1);
  }

  void _reset(model* v)
  {
    if (_self != nullptr && _self->count.fetch_sub(1, std::memory_order_release) == 1) {
      std::atomic_thread_fence(std::memory_order_acquire);
      _destroy_model(_alloc, _self);
    }
    _self = v;
  }

  [[no_unique_address]] allocator_type _alloc;
  model* _self;
};

/// @brief Equality comparison of two `copy_on_write` objects.
///
/// If both operands are valueless they are considered equal. If one is valueless
/// and the other is not, they are considered unequal. Objects that share the
/// same internal storage (i.e. `x.identical_to(y)`) always compare equal
/// without dereferencing the managed objects. When the objects do not share
/// storage, the comparison delegates to the equality operator of the managed
/// objects themselves.
///
/// @tparam T1 Value type of @p x.
/// @tparam A1 Allocator type of @p x.
/// @tparam T2 Value type of @p y.
/// @tparam A2 Allocator type of @p y.
/// @param x The left-hand operand.
/// @param y The right-hand operand.
/// @return `true` if the managed objects compare equal or both objects are
///         valueless; `false` otherwise.
template <typename T1, typename A1, typename T2, typename A2>
auto operator==(copy_on_write<T1, A1> const& x,
                copy_on_write<T2, A2> const& y) noexcept(noexcept(*x == *y)) -> bool
{
  if (x.valueless_after_move() || y.valueless_after_move()) {
    return x.valueless_after_move() == y.valueless_after_move();
  }
  if constexpr (std::same_as<T1, T2> && std::same_as<A1, A2>) {
    if (x.identical_to(y)) {
      return true;
    }
  }
  return *x == *y;
}

/// @brief Equality comparison of a `copy_on_write` object and a non-`copy_on_write` value.
///
/// Returns `false` if @p x is valueless.
///
/// @tparam T Value type of @p x.
/// @tparam A Allocator type of @p x.
/// @tparam U Type of the right-hand operand. Must not be a `copy_on_write` specialisation.
/// @param x The left-hand operand.
/// @param y The right-hand operand.
/// @return `true` if @p x is not valueless and `*x == y`.
template <typename T, typename A, typename U>
  requires(!detail::is_copy_on_write_v<U>)
auto operator==(copy_on_write<T, A> const& x, U const& y) noexcept(noexcept(*x == y)) -> bool
{
  return !x.valueless_after_move() && (*x == y);
}

/// @brief Three-way comparison of two `copy_on_write` objects.
///
/// A valueless object compares less than a non-valueless object. Two valueless
/// objects compare equal. Objects sharing the same storage always compare equal.
///
/// @tparam T1 Value type of @p x.
/// @tparam A1 Allocator type of @p x.
/// @tparam T2 Value type of @p y.
/// @tparam A2 Allocator type of @p y.
/// @param x The left-hand operand.
/// @param y The right-hand operand.
/// @return The result of the three-way comparison.
template <typename T1, typename A1, typename T2, typename A2>
auto operator<=>(copy_on_write<T1, A1> const& x, copy_on_write<T2, A2> const& y)
  -> detail::synth_three_way_result<T1, T2>
{
  if (x.valueless_after_move() || y.valueless_after_move()) {
    return !x.valueless_after_move() <=> !y.valueless_after_move();
  }
  if constexpr (std::same_as<T1, T2> && std::same_as<A1, A2>) {
    if (x.identical_to(y)) {
      return std::strong_ordering::equal;
    }
  }
  return detail::synth_three_way(*x, *y);
}

/// @brief Three-way comparison of a `copy_on_write` object and a non-`copy_on_write` value.
///
/// A valueless @p x compares as `std::strong_ordering::less` relative to any
/// value of @p y.
///
/// @tparam T Value type of @p x.
/// @tparam A Allocator type of @p x.
/// @tparam U Type of the right-hand operand. Must not be a `copy_on_write` specialisation.
/// @param x The left-hand operand.
/// @param y The right-hand operand.
/// @return The result of the three-way comparison.
template <typename T, typename A, typename U>
  requires(!detail::is_copy_on_write_v<U>)
auto operator<=>(copy_on_write<T, A> const& x, U const& y) -> detail::synth_three_way_result<T, U>
{
  if (x.valueless_after_move()) {
    return std::strong_ordering::less;
  }
  return detail::synth_three_way(*x, y);
}

/// @brief Swaps two `copy_on_write` objects.
///
/// Equivalent to `x.swap(y)`. Participates in overload resolution for
/// `std::swap`.
///
/// @tparam T The value type.
/// @tparam A The allocator type.
/// @param x The first object.
/// @param y The second object.
template <typename T, typename A>
void swap(copy_on_write<T, A>& x, copy_on_write<T, A>& y) noexcept(noexcept(x.swap(y)))
{
  x.swap(y);
}

/// @brief Deduction guide: deduces `copy_on_write<Value>` from a single value
///        argument.
template <typename Value>
copy_on_write(Value) -> copy_on_write<Value>;

/// @brief Deduction guide: deduces `copy_on_write<Value, ReboundAllocator>`
///        from an `allocator_arg_t` tag, an allocator, and a value argument.
template <typename Allocator, typename Value>
copy_on_write(std::allocator_arg_t, Allocator, Value)
  -> copy_on_write<Value, typename std::allocator_traits<Allocator>::template rebind_alloc<Value>>;

/// @namespace xyz::pmr
/// @brief Convenience aliases using `std::pmr::polymorphic_allocator`.
namespace pmr {

/// @brief Alias for `xyz::copy_on_write<T>` that uses
///        `std::pmr::polymorphic_allocator<T>` as the allocator.
///
/// @tparam T The type of the managed object.
template <typename T>
using copy_on_write = xyz::copy_on_write<T, std::pmr::polymorphic_allocator<T>>;

} // namespace pmr
} // namespace xyz

/// @brief Specialisation of `std::hash` for `xyz::copy_on_write<T, Allocator>`.
///
/// Enabled only when @p T is hashable (i.e. `std::hash<T>` is well-formed).
/// A valueless object hashes to `static_cast<std::size_t>(-1)`; otherwise the
/// hash of the managed object is returned.
///
/// @tparam T         The value type of the `copy_on_write` object.
/// @tparam Allocator The allocator type of the `copy_on_write` object.
template <typename T, typename Allocator>
  requires xyz::detail::hashable<T>
struct std::hash<xyz::copy_on_write<T, Allocator>>
{
  /// @brief Computes the hash of the `copy_on_write` object.
  ///
  /// @param x The object to hash.
  /// @return The hash value. Returns `static_cast<std::size_t>(-1)` if @p x is
  ///         valueless.
  constexpr std::size_t operator()(xyz::copy_on_write<T, Allocator> const& x) const
    noexcept(noexcept(std::hash<T>{}(*x)))
  {
    if (x.valueless_after_move()) {
      return static_cast<std::size_t>(-1);
    }
    return std::hash<T>{}(*x);
  }
};

#endif
