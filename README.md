# `copy_on_write`: A Vocabulary Type for Lazily-Copied Values

ISO/IEC JTC1 SC22 WG21 Programming Language C++

DXXXXR0

Working Group: Library Evolution, Library

Date: 2026-XX-XX

_Daniel Pfeifer \<daniel@pfeifer-mail.de>_

# Abstract

We propose the addition of a new class template `copy_on_write<T>` to the C++
Standard Library.

`copy_on_write<T>` manages a dynamically-allocated owned object of type `T` with
shared, reference-counted ownership. Copies of a `copy_on_write<T>` share
ownership of the same underlying object; the object is copied only when a
modification is requested and the reference count exceeds one. This gives
efficient value semantics: copying is O(1) regardless of the size or complexity
of `T`, and mutation is deferred until necessary.

Specializations of `copy_on_write` have value semantics and compose well with
other standard library types such as `vector`, allowing the compiler to
correctly generate special member functions.

Access to the owned object through `operator*` and `operator->` is always
const-qualified. Mutation of the owned object is performed exclusively through
the `modify` member function, which copies the shared data when necessary and
provides a structured interface for efficient in-place or constructive
modification.

# Motivation

## Expensive Copies in Composite Classes

When composite objects contain large or expensive-to-copy members, value
semantics become costly. Consider a spell-checker that maintains a dictionary
as a sorted list of words:

```cpp
class dictionary {
  std::vector<std::string> _words;
public:
  dictionary() = default;
  void add_word(std::string word) {
    auto it = std::lower_bound(_words.begin(), _words.end(), word);
    _words.insert(it, std::move(word));
  }
  bool contains(std::string_view word) const {
    return std::binary_search(_words.begin(), _words.end(), word);
  }
};
```

A spell-checker might load a shared system dictionary and give each open
document its own copy, so that custom words added in one document do not appear
in another. Since most documents never add any custom words, creating a full
deep copy of the entire word list for each document is wasteful.

`copy_on_write<T>` provides a middle ground. Multiple documents share the
underlying word list, paying only an atomic reference-count increment when a
new document is opened. A copy of the word list is made only when a document
actually adds a custom word, which is the uncommon case.

## Composing Cheaply-Copyable Types

`copy_on_write<T>` allows composite classes to be cheaply copyable even when
their components are expensive to copy:

```cpp
class dictionary {
  std::copy_on_write<std::vector<std::string>> _words;
public:
  dictionary() = default;

  // Copying is O(1): just an atomic increment.
  dictionary(const dictionary&) noexcept = default;
  dictionary& operator=(const dictionary&) noexcept = default;

  // Mutation copies the underlying data only if shared.
  void add_word(std::string word) {
    _words.modify([&](std::vector<std::string>& v) {
      auto it = std::lower_bound(v.begin(), v.end(), word);
      v.insert(it, std::move(word));
    });
  }

  bool contains(std::string_view word) const {
    return std::binary_search(_words->begin(), _words->end(), word);
  }
};
```

The class above has full value semantics — compiler-generated copy, move, and
assignment work correctly — while avoiding expensive deep copies when the
dictionary is only being read.

## Efficient Fork-and-Edit Patterns

`copy_on_write<T>` is well suited to fork-and-edit workflows where a derived
value is created from an existing one with few changes:

```cpp
std::copy_on_write<Config> cfg = base_config();
std::copy_on_write<Config> debug_cfg(cfg);  // O(1), shared
debug_cfg.modify([](Config& c) {
  c.set_log_level(LogLevel::Debug);
});
// cfg is unchanged; debug_cfg holds a private copy.
```

Without `copy_on_write`, the second line would require a full deep copy of
`Config` regardless of whether it is ever modified.

## Avoiding Redundant Work on Shared Data

When two `copy_on_write<T>` objects share the same underlying data, they are
provably equal. The `identical_to` member function and equality comparison
short-circuit in this case, avoiding potentially expensive element-wise
comparisons.

# Design Requirements

## Shared Ownership with Value Semantics

Unlike `indirect<T>`, which has unique ownership, `copy_on_write<T>` uses
shared, reference-counted ownership. The reference count is maintained
atomically to allow copies to be safely shared across threads.

Despite shared ownership of the underlying storage, `copy_on_write<T>` presents
value semantics to its users: two `copy_on_write<T>` objects are equal when
their owned objects are equal, and modifying one does not affect the other.

## Cheap Copies

Copying a `copy_on_write<T>` increments the reference count and shares a pointer
to the owned object. The cost is O(1) and independent of the size or complexity
of `T`. The owned object is copied only when `modify` is called and the
reference count exceeds one.

## Read-Only Access via Observers

`copy_on_write<T>` provides only const access to the owned object through
`operator*` and `operator->`. Mutable access is not available directly. This
invariant is necessary for correctness: if a mutable reference to the shared
object were handed out without a preceding copy, modifications through that
reference would be visible to all other objects sharing the same data, violating
value semantics.

When a `copy_on_write<T>` is accessed through a const access path, constness
propagates to the owned object. Because all access is already const-qualified,
this is automatically satisfied.

## Modification Through `modify`

Modification of the owned object is performed through the `modify` member
function. Three overloads are provided:

**`modify(action)`** accepts a callable of the form `void(T&)`. If the reference
count is one (exclusive ownership), the action is applied directly to the owned
object. If the reference count exceeds one (shared ownership), the owned object
is first copied, ownership becomes exclusive, and then the action is applied.

**`modify(action, transform)`** accepts both a `void(T&)` action and a
`T(T const&)` transformation. If the reference count is one, the action is
applied in-place. If the reference count exceeds one, a new owned object is
constructed by calling the transformation on the current value, without first
copying the old value. This allows the caller to provide an optimised
construction path for the shared case.

For example, when inserting an element at the front of a vector: the in-place
action calls `insert` on the existing vector; the transformation constructs a
new vector from scratch, placing the new element first and then appending the
remaining elements. In the shared case this avoids the cost of first copying the
full vector and then inserting.

**`modify(transform)`** accepts a `T(T const&)` transformation. If the reference
count is one, the result of the transformation is assigned back to the owned
object, potentially reusing its storage. If the reference count exceeds one, a
new owned object is constructed by calling the transformation on the current
value.

## Identity Optimisation

Two `copy_on_write<T>` objects that share the same underlying object are
provably equal. The `identical_to` member function returns `true` when two
objects of the same specialisation share the same owned object. Equality
comparison between two objects of the same specialisation short-circuits to
`true` when `identical_to` returns `true`, avoiding a potentially expensive
value comparison.

Users may also call `identical_to` directly to short-circuit work in algorithms
that operate on `copy_on_write<T>` values.

## The Valueless State and Interaction with `std::optional`

`copy_on_write<T>` has a valueless state used to implement move. A
`copy_on_write<T>` object becomes valueless only after it has been moved from.
Accessing the owned object of a valueless object is undefined behaviour. The
`valueless_after_move` member function returns `true` if the object is in the
valueless state.

The valueless state is not intended to be observable in normal use. There is no
`operator bool` or `has_value` member function.

Where a nullable `copy_on_write<T>` is required,
`std::optional<copy_on_write<T>>` is recommended.

## Allocator Support

`copy_on_write<T>` is allocator-aware. The allocator is used to allocate and
deallocate storage for an internal control block that holds both the reference
count and the owned object.

When two `copy_on_write<T>` objects have equal allocators, a copy shares the
underlying data; when allocators differ, a deep copy is made. This ensures that
an object created with one memory resource is not owned by another.

## Special Member Functions

`copy_on_write<T>` is suitable for use as a member of composite classes where
the compiler will generate special member functions. Copy construction and copy
assignment are available whenever `T` is copy constructible. Move construction
is unconditionally available and noexcept.

# Prior Work

The copy-on-write idiom has a long history in C++. Qt's `QSharedDataPointer` (Qt
4.0, 2005) pioneered a reusable COW pointer type in a major C++ framework.
Adobe's `stlab` library provides a `copy_on_write` type along similar lines. The
GCC implementation of `std::basic_string` prior to C++11 used copy-on-write
semantics internally; it was later removed due to incompatibilities with the
C++11 memory model and iterator invalidation requirements.

This proposal standardises a modern, allocator-aware formulation aligned with
the design of `std::indirect` [P3019].

# Impact on the Standard

This proposal is a pure library extension. It requires additions to be made to
the standard library header `<memory>`.

# Technical Specifications

## Header `<version>` synopsis [version.syn]

Note to editors: Add the following macro with an editor-provided value to
\[version.syn\]:

```cpp
#define __cpp_lib_copy_on_write ??????L  // also in <memory>
```

## Header `<memory>` synopsis [memory]

Add the following to the `<memory>` synopsis within `namespace std`:

```cpp
// [cow], class template copy_on_write
template <class T, class Allocator = allocator<T>>
  class copy_on_write;

// [cow.hash], hash support
template <class T, class Alloc>
  struct hash<copy_on_write<T, Alloc>>;

namespace pmr {
  template <class T>
    using copy_on_write =
      std::copy_on_write<T, polymorphic_allocator<T>>;
}
```

## X.Y Class template copy_on_write [cow]

### X.Y.1 General [cow.general]

- A `copy_on_write` object manages the lifetime of an owned object. A
  `copy_on_write` object is _valueless_ if it has no owned object. A
  `copy_on_write` object may become valueless only after it has been moved from.

- Multiple `copy_on_write` objects may share ownership of the same owned object.
  A reference count is maintained to track the number of objects sharing
  ownership. The reference count is modified atomically. When the reference
  count reaches zero, the owned object is destroyed and its storage is
  deallocated.

- In every specialization `copy_on_write<T, Allocator>`, if the type
  `allocator_traits<Allocator>::value_type` is not the same type as `T`, the
  program is ill-formed. Every object of type `copy_on_write<T, Allocator>` uses
  an object of type `Allocator` to allocate and free storage for the owned
  object as needed.

- A program that instantiates the definition of the template
  `copy_on_write<T, Allocator>` with a non-object type, an array type,
  `in_place_t`, a specialization of `in_place_type_t`, or a cv-qualified type
  for the `T` parameter is ill-formed.

- The template parameter `T` of `copy_on_write` may be an incomplete type.

- The template parameter `Allocator` of `copy_on_write` shall meet the
  _Cpp17Allocator_ requirements.

- If a program declares an explicit or partial specialization of
  `copy_on_write`, the behavior is undefined.

The following exposition-only concepts are used in the specification. Let `F` be
a type and `T` a value type.

- _`cow-action<F, T>`_ is satisfied when `F` is invocable with an lvalue of type
  `T` and the result type is `void`.

- _`cow-transformation<F, T>`_ is satisfied when `F` is invocable with a const
  lvalue of type `T` and the result type is `T`.

### X.Y.2 Synopsis [cow.syn]

```cpp
template <class T, class Allocator = allocator<T>>
class copy_on_write {
 public:
  using value_type     = T;
  using allocator_type = Allocator;
  using const_pointer  =
    typename allocator_traits<Allocator>::const_pointer;

  explicit copy_on_write();

  explicit copy_on_write(
    allocator_arg_t, const Allocator& a);

  template <class U = T>
  explicit copy_on_write(U&& u);

  template <class U = T>
  explicit copy_on_write(
    allocator_arg_t, const Allocator& a, U&& u);

  template <class... Us>
  explicit copy_on_write(in_place_t, Us&&... us);

  template <class... Us>
  explicit copy_on_write(
    allocator_arg_t, const Allocator& a,
    in_place_t, Us&&... us);

  template <class I, class... Us>
  explicit copy_on_write(
    in_place_t,
    initializer_list<I> ilist, Us&&... us);

  template <class I, class... Us>
  explicit copy_on_write(
    allocator_arg_t, const Allocator& a,
    in_place_t,
    initializer_list<I> ilist, Us&&... us);

  explicit copy_on_write(
    allocator_arg_t, const Allocator& a,
    const copy_on_write& other);

  explicit copy_on_write(
    allocator_arg_t, const Allocator& a,
    copy_on_write&& other)
    noexcept(
      allocator_traits<Allocator>::is_always_equal::value);

  copy_on_write(const copy_on_write& other);

  copy_on_write(copy_on_write&& other) noexcept;

  ~copy_on_write();

  copy_on_write& operator=(const copy_on_write& other);

  copy_on_write& operator=(copy_on_write&& other)
    noexcept(
      allocator_traits<Allocator>
        ::propagate_on_container_move_assignment::value
      || allocator_traits<Allocator>
           ::is_always_equal::value);

  template <class U = T>
  copy_on_write& operator=(U&& u);

  const T& operator*() const noexcept;

  const_pointer operator->() const noexcept;

  bool valueless_after_move() const noexcept;

  allocator_type get_allocator() const noexcept;

  long use_count() const noexcept;

  bool identical_to(
    const copy_on_write& other) const noexcept;

  template <class F>  // requires cow-action<F, T>
  void modify(F&& f);

  template <class F, class G>
    // requires cow-action<F, T>
    //       && cow-transformation<G, T>
  void modify(F&& f, G&& g);

  template <class F>  // requires cow-transformation<F, T>
  void modify(F&& f);

  void swap(copy_on_write& other) noexcept(see below);

  friend void swap(
    copy_on_write& lhs,
    copy_on_write& rhs) noexcept(noexcept(lhs.swap(rhs)));

  template <class U, class AA>
  friend bool operator==(
    const copy_on_write& lhs,
    const copy_on_write<U, AA>& rhs)
    noexcept(see below);

  template <class U>
  friend bool operator==(
    const copy_on_write& lhs,
    const U& rhs) noexcept(see below);

  template <class U, class AA>
  friend auto operator<=>(
    const copy_on_write& lhs,
    const copy_on_write<U, AA>& rhs)
    -> synth-three-way-result<T, U>;

  template <class U>
  friend auto operator<=>(
    const copy_on_write& lhs,
    const U& rhs)
    -> synth-three-way-result<T, U>;

 private:
  Allocator alloc = Allocator();  // exposition only
  /* unspecified */ p;            // exposition only
};

template <class Value>
copy_on_write(Value) -> copy_on_write<Value>;

template <class Allocator, class Value>
copy_on_write(allocator_arg_t, Allocator, Value)
  -> copy_on_write<Value,
       typename allocator_traits<Allocator>
         ::template rebind_alloc<Value>>;
```

### X.Y.3 Constructors [cow.ctor]

The following element applies to all functions in \[cow.ctor\]:

_Throws_: Nothing unless `allocator_traits<Allocator>::allocate` or
`allocator_traits<Allocator>::construct` throws.

```cpp
explicit copy_on_write();
```

- _Constraints_: `is_default_constructible_v<Allocator>` is `true`.
- _Mandates_: `is_default_constructible_v<T>` is `true`.
- _Effects_: Constructs an owned object of type `T` with an empty argument list,
  using the allocator `alloc`.

```cpp
explicit copy_on_write(allocator_arg_t, const Allocator& a);
```

- _Mandates_: `is_default_constructible_v<T>` is `true`.
- _Effects_: `alloc` is direct-non-list-initialized with `a`. Constructs an
  owned object of type `T` with an empty argument list, using the allocator
  `alloc`.

```cpp
template <class U = T>
explicit copy_on_write(U&& u);
```

- _Constraints_:
  - `is_same_v<remove_cvref_t<U>, copy_on_write>` is `false`,
  - `is_same_v<remove_cvref_t<U>, in_place_t>` is `false`,
  - `is_constructible_v<T, U>` is `true`, and
  - `is_default_constructible_v<Allocator>` is `true`.
- _Effects_: Constructs an owned object of type `T` with `std::forward<U>(u)`,
  using the allocator `alloc`.

```cpp
template <class U = T>
explicit copy_on_write(
  allocator_arg_t, const Allocator& a, U&& u);
```

- _Constraints_:
  - `is_same_v<remove_cvref_t<U>, copy_on_write>` is `false`,
  - `is_same_v<remove_cvref_t<U>, in_place_t>` is `false`, and
  - `is_constructible_v<T, U>` is `true`.
- _Effects_: `alloc` is direct-non-list-initialized with `a`. Constructs an
  owned object of type `T` with `std::forward<U>(u)`, using the allocator
  `alloc`.

```cpp
template <class... Us>
explicit copy_on_write(in_place_t, Us&&... us);
```

- _Constraints_:
  - `is_constructible_v<T, Us...>` is `true`, and
  - `is_default_constructible_v<Allocator>` is `true`.
- _Effects_: Constructs an owned object of type `T` with
  `std::forward<Us>(us)...`, using the allocator `alloc`.

```cpp
template <class... Us>
explicit copy_on_write(
  allocator_arg_t, const Allocator& a,
  in_place_t, Us&&... us);
```

- _Constraints_: `is_constructible_v<T, Us...>` is `true`.
- _Effects_: `alloc` is direct-non-list-initialized with `a`. Constructs an
  owned object of type `T` with `std::forward<Us>(us)...`, using the allocator
  `alloc`.

```cpp
template <class I, class... Us>
explicit copy_on_write(
  in_place_t,
  initializer_list<I> ilist, Us&&... us);
```

- _Constraints_:
  - `is_constructible_v<T, initializer_list<I>&, Us...>` is `true`, and
  - `is_default_constructible_v<Allocator>` is `true`.
- _Effects_: Constructs an owned object of type `T` with
  `ilist, std::forward<Us>(us)...`, using the allocator `alloc`.

```cpp
template <class I, class... Us>
explicit copy_on_write(
  allocator_arg_t, const Allocator& a,
  in_place_t,
  initializer_list<I> ilist, Us&&... us);
```

- _Constraints_: `is_constructible_v<T, initializer_list<I>&, Us...>` is `true`.
- _Effects_: `alloc` is direct-non-list-initialized with `a`. Constructs an
  owned object of type `T` with `ilist, std::forward<Us>(us)...`, using the
  allocator `alloc`.

```cpp
explicit copy_on_write(
  allocator_arg_t, const Allocator& a,
  const copy_on_write& other);
```

- _Mandates_: `is_copy_constructible_v<T>` is `true`.
- _Effects_: `alloc` is direct-non-list-initialized with `a`. If `other` is
  valueless, `*this` is valueless. Otherwise, if `a == other.alloc` is `true`,
  `*this` shares ownership of `other`'s owned object and the reference count is
  incremented. Otherwise, constructs an owned object of type `T` with `*other`,
  using the allocator `alloc`.

```cpp
explicit copy_on_write(
  allocator_arg_t, const Allocator& a,
  copy_on_write&& other)
  noexcept(
    allocator_traits<Allocator>::is_always_equal::value);
```

- _Effects_: `alloc` is direct-non-list-initialized with `a`. If `other` is
  valueless, `*this` is valueless. Otherwise, if `a == other.alloc` is `true`,
  `*this` takes exclusive ownership of the owned object of `other`. Otherwise,
  constructs an owned object of type `T` with `std::move(*other)`, using the
  allocator `alloc`.
- _Postconditions_: `other` is valueless.

```cpp
copy_on_write(const copy_on_write& other);
```

- _Mandates_: `is_copy_constructible_v<T>` is `true`.
- _Effects_: `alloc` is direct-non-list-initialized with
  `allocator_traits<Allocator>::   select_on_container_copy_construction(other.alloc)`.
  If `other` is valueless, `*this` is valueless. Otherwise, if
  `alloc == other.alloc` is `true`, `*this` shares ownership of `other`'s owned
  object and the reference count is incremented. Otherwise, constructs an owned
  object of type `T` with `*other`, using the allocator `alloc`.

```cpp
copy_on_write(copy_on_write&& other) noexcept;
```

- _Effects_: `alloc` is direct-non-list-initialized with `other.alloc`. If
  `other` is valueless, `*this` is valueless. Otherwise, `*this` takes exclusive
  ownership of the owned object of `other`.
- _Postconditions_: `other` is valueless.

### X.Y.4 Destructor [cow.dtor]

```cpp
~copy_on_write();
```

- _Mandates_: `T` is a complete type.
- _Effects_: Decrements the reference count of the owned object. If the
  reference count reaches zero, destroys the owned object using
  `allocator_traits<Allocator>::destroy` and deallocates the storage.

### X.Y.5 Assignment [cow.assign]

```cpp
copy_on_write& operator=(const copy_on_write& other);
```

- _Mandates_: `is_copy_constructible_v<T>` is `true`.

- _Effects_: If `addressof(other) == this` is `true`, there are no effects.
  Otherwise:

  1. The allocator needs updating if
     `allocator_traits<Allocator>::   propagate_on_container_copy_assignment::value`
     is `true`.

  2. If `other` is valueless, `*this` becomes valueless. The reference count of
     the previously-owned object in `*this`, if any, is decremented; if it
     reaches zero, the owned object is destroyed and storage is deallocated.

  3. Otherwise, if the allocator does not need updating and
     `alloc == other.alloc` is `true`, `*this` shares ownership of `other`'s
     owned object. The reference count is incremented.

  4. Otherwise, constructs a new owned object of type `T` with `*other`, using
     the new allocator.

  5. The reference count of the previously-owned object in `*this`, if any, is
     decremented; if it reaches zero, the owned object is destroyed and storage
     is deallocated.

  6. If the allocator needs updating, the allocator in `*this` is replaced with
     a copy of the allocator in `other`.

- _Returns_: A reference to `*this`.

```cpp
copy_on_write& operator=(copy_on_write&& other)
  noexcept(
    allocator_traits<Allocator>
      ::propagate_on_container_move_assignment::value
    || allocator_traits<Allocator>::is_always_equal::value);
```

- _Effects_: If `addressof(other) == this` is `true`, there are no effects.
  Otherwise:

  1. The allocator needs updating if
     `allocator_traits<Allocator>::propagate_on_container_move_assignment::value`
     is `true`.

  2. If `other` is valueless, `*this` becomes valueless. The reference count of
     the previously-owned object in `*this`, if any, is decremented; if it
     reaches zero, the owned object is destroyed and storage is deallocated.

  3. Otherwise, if the allocator does not need updating and
     `alloc == other.alloc` is `true`, `*this` takes exclusive ownership of the
     owned object of `other`.

  4. Otherwise, constructs a new owned object with `std::move(*other)`, using
     the allocator `alloc`.

  5. The reference count of the previously-owned object in `*this`, if any, is
     decremented; if it reaches zero, the owned object is destroyed and storage
     is deallocated.

  6. If the allocator needs updating, the allocator in `*this` is replaced with
     a copy of the allocator in `other`.

- _Postconditions_: `other` is valueless.

- _Returns_: A reference to `*this`.

```cpp
template <class U = T>
copy_on_write& operator=(U&& u);
```

- _Constraints_:
  - `is_same_v<remove_cvref_t<U>, copy_on_write>` is `false`,
  - `is_constructible_v<T, U>` is `true`, and
  - `is_assignable_v<T&, U>` is `true`.
- _Effects_: If `*this` is not valueless and `use_count() == 1`, equivalent to
  `**this = std::forward<U>(u)`. Otherwise, constructs a new `copy_on_write<T>`
  from `std::forward<U>(u)` and move-assigns it to `*this`.
- _Returns_: A reference to `*this`.

### X.Y.6 Observers [cow.obs]

```cpp
const T& operator*() const noexcept;
```

- _Preconditions_: `*this` is not valueless.
- _Returns_: A const reference to the owned object.

```cpp
const_pointer operator->() const noexcept;
```

- _Preconditions_: `*this` is not valueless.
- _Returns_: A pointer to the owned object.

```cpp
bool valueless_after_move() const noexcept;
```

- _Returns_: `true` if `*this` is valueless, otherwise `false`.

```cpp
allocator_type get_allocator() const noexcept;
```

- _Returns_: `alloc`.

```cpp
long use_count() const noexcept;
```

- _Preconditions_: `*this` is not valueless.
- _Returns_: The number of `copy_on_write` objects currently sharing ownership
  of the owned object.
- _Remarks_: In the presence of concurrent access, the returned value may be
  stale by the time it is observed. It should be treated as an optimisation hint
  rather than a precise count.

```cpp
bool identical_to(const copy_on_write& other) const noexcept;
```

- _Preconditions_: Neither `*this` nor `other` is valueless.
- _Returns_: `true` if `*this` and `other` share the same underlying owned
  object, otherwise `false`.

### X.Y.7 Modifiers [cow.mod]

```cpp
template <class F>  // requires cow-action<F, T>
void modify(F&& f);
```

- _Constraints_: _`cow-action<F, T>`_ is satisfied.
- _Preconditions_: `*this` is not valueless.
- _Effects_: If `use_count() == 1`, calls `std::forward<F>(f)` with a mutable
  reference to the owned object. Otherwise, creates a new owned object as a copy
  of the current owned object (using the allocator `alloc`), decrements the
  reference count of the previous owned object, takes exclusive ownership of the
  new object, and calls `std::forward<F>(f)` with a mutable reference to the new
  owned object.

```cpp
template <class F, class G>
  // requires cow-action<F, T> && cow-transformation<G, T>
void modify(F&& f, G&& g);
```

- _Constraints_: _`cow-action<F, T>`_ and _`cow-transformation<G, T>`_ are both
  satisfied.
- _Preconditions_: `*this` is not valueless.
- _Effects_: If `use_count() == 1`, calls `std::forward<F>(f)` with a mutable
  reference to the owned object. Otherwise, creates a new owned object by
  calling `std::forward<G>(g)` with a const reference to the current owned
  object (using the allocator `alloc`), decrements the reference count of the
  previous owned object, and takes exclusive ownership of the new object.
- _Remarks_: In the shared case, the transformation `g` is responsible for
  constructing the new value. Unlike the single-argument `modify(action)`
  overload, the old owned object is not copied before the new one is created;
  `g` receives only a const reference. This allows callers to construct the new
  value more efficiently than copy-then-modify.

```cpp
template <class F>  // requires cow-transformation<F, T>
void modify(F&& f);
```

- _Constraints_: _`cow-transformation<F, T>`_ is satisfied.
- _Preconditions_: `*this` is not valueless.
- _Effects_: If `use_count() == 1`, assigns
  `std::forward<F>(f)(std::as_const(*this_value))` to the owned object, where
  `this_value` is a pointer to the owned object. Otherwise, creates a new owned
  object by calling `std::forward<F>(f)` with a const reference to the current
  owned object (using the allocator `alloc`), decrements the reference count of
  the previous owned object, and takes exclusive ownership of the new object.

### X.Y.8 Swap [cow.swap]

```cpp
void swap(copy_on_write& other)
  noexcept(
    allocator_traits<Allocator>
      ::propagate_on_container_swap::value
    || allocator_traits<Allocator>::is_always_equal::value);
```

- _Preconditions_: If
  `allocator_traits<Allocator>::   propagate_on_container_swap::value` is
  `false`, then `get_allocator() == other.get_allocator()` is `true`.
- _Effects_: Exchanges the owned objects or valueless states of `*this` and
  `other`. If
  `allocator_traits<Allocator>::   propagate_on_container_swap::value` is
  `true`, the allocators of `*this` and `other` are exchanged. Otherwise the
  allocators are not swapped. \[_Note_: Does not call `swap` on the owned
  objects directly. —_end note_\]

```cpp
friend void swap(
  copy_on_write& lhs,
  copy_on_write& rhs) noexcept(noexcept(lhs.swap(rhs)));
```

- _Effects_: Equivalent to `lhs.swap(rhs)`.

### X.Y.9 Relational operators [cow.relops]

```cpp
template <class U, class AA>
friend bool operator==(
  const copy_on_write& lhs,
  const copy_on_write<U, AA>& rhs)
  noexcept(noexcept(*lhs == *rhs));
```

- _Mandates_: The expression `*lhs == *rhs` is well-formed and its result is
  convertible to `bool`.
- _Returns_: If `lhs` is valueless or `rhs` is valueless,
  `lhs.valueless_after_move() == rhs.valueless_after_move()`. Otherwise, if `T`,
  `U`, `Allocator`, and `AA` are the same types and `lhs.identical_to(rhs)` is
  `true`, `true`. Otherwise, `*lhs == *rhs`.

```cpp
template <class U, class AA>
friend auto operator<=>(
  const copy_on_write& lhs,
  const copy_on_write<U, AA>& rhs)
  -> synth-three-way-result<T, U>;
```

- _Returns_: If `lhs` is valueless or `rhs` is valueless,
  `!lhs.valueless_after_move() <=> !rhs.valueless_after_move()`. Otherwise, if
  `T`, `U`, `Allocator`, and `AA` are the same types and `lhs.identical_to(rhs)`
  is `true`, `strong_ordering::equal`. Otherwise, `synth-three-way(*lhs, *rhs)`.

### X.Y.10 Comparison with T [cow.comp.with.t]

```cpp
template <class U>
friend bool operator==(
  const copy_on_write& lhs,
  const U& rhs) noexcept(noexcept(*lhs == rhs));
```

- _Mandates_: The expression `*lhs == rhs` is well-formed and its result is
  convertible to `bool`. `U` is not a specialization of `copy_on_write`.
- _Returns_: If `lhs` is valueless, `false`; otherwise `*lhs == rhs`.

```cpp
template <class U>
friend auto operator<=>(
  const copy_on_write& lhs,
  const U& rhs)
  -> synth-three-way-result<T, U>;
```

- _Mandates_: `U` is not a specialization of `copy_on_write`.
- _Returns_: If `lhs` is valueless, `strong_ordering::less`; otherwise
  `synth-three-way(*lhs, rhs)`.

### X.Y.11 Hash support [cow.hash]

```cpp
template <class T, class Allocator>
struct hash<copy_on_write<T, Allocator>>;
```

The specialization `hash<copy_on_write<T, Allocator>>` is enabled ([unord.hash])
if and only if `hash<T>` is enabled. When enabled, for an object `c` of type
`copy_on_write<T, Allocator>`, `hash<copy_on_write<T, Allocator>>()(c)`
evaluates to `hash<T>()(*c)` if `c` is not valueless, otherwise to an
implementation-defined value. The member functions are not guaranteed to be
noexcept.

# Design Discussions

## Read-Only Access and the `modify` Interface

The absence of a non-const `operator*` and `operator->` is the most distinctive
aspect of `copy_on_write` compared to `indirect`.

Providing a `T&` directly would require one of two things: either eagerly copy
the shared object at the point of access (eliminating the copy-on-write
optimisation entirely), or return a `T&` to the shared object without copying.
The latter would allow the caller to modify data visible to all other owners,
violating value semantics.

The `modify` interface resolves this tension. By accepting a callable, it can
determine at the point of invocation whether a copy is needed (when
`use_count() > 1`) or whether in-place modification is safe (when
`use_count() == 1`).

The two-argument overload `modify(action, transform)` further allows callers to
provide a more efficient algorithm for the shared case. When ownership is not
exclusive and the old object must be discarded, constructing the new value from
scratch (via `transform`) is often more efficient than copy-constructing and
then modifying. The caller is in the best position to provide this optimisation.

## Thread Safety

The reference count is maintained with atomic operations, making it safe to copy
and destroy `copy_on_write<T>` objects concurrently from different threads.
However, concurrent reads of the owned object are safe only in the usual sense:
multiple threads may call `operator*` and `operator->` simultaneously on objects
sharing the same underlying data. Concurrent calls to `modify` on distinct
objects that share data are safe; the first caller to modify will detach its
copy, leaving the other objects undisturbed.

Concurrent calls to `modify` on the _same_ `copy_on_write<T>` object, or
concurrent read and write of the same object, require external synchronisation,
as with any mutable standard library type.

## Interaction with `shared_ptr`

`copy_on_write<T>` has a similar ownership model to `shared_ptr<const T>`:
multiple owners, atomic reference counting. It differs in several important
respects:

- The owned object is never accessible through a mutable pointer, eliminating
  the possibility of inadvertent mutation of shared state.
- Mutation is performed through the first-class `modify` interface rather than
  being entirely absent.
- `copy_on_write` has value semantics: it is copy-assignable and participates in
  value-based equality comparison and hashing.
- `copy_on_write` has no null state and does not support pointer arithmetic or
  custom deleters.
- `copy_on_write` is allocator-aware in the standard manner.

## `use_count` and `identical_to`

`use_count` and `identical_to` expose internal sharing state and are provided
primarily as optimisation hints, analogous to `shared_ptr::use_count`. Their
values may change concurrently when other objects sharing the same data are
copied or destroyed. Programs should not rely on the exact value of `use_count`
for correctness in the presence of concurrent access.

`identical_to` is additionally used internally to provide an O(1) fast path in
equality comparison: two objects sharing the same underlying data are provably
equal without inspecting their values.

## Explicit Constructors

All constructors of `copy_on_write` are marked `explicit`. Using
dynamically-allocated shared storage is a deliberate choice that should not
occur implicitly. This is consistent with the design of `indirect` and
`polymorphic` [P3019].

## Interaction with `indirect`

`copy_on_write<T>` and `indirect<T>` are complementary vocabulary types for
dynamically-allocated values:

| Property | `indirect<T>` | `copy_on_write<T>` |
|----------------------------|----------------|--------------------| | Ownership
| Unique | Shared (COW) | | Copy cost | O(sizeof T) | O(1) | | Mutable access |
Yes | Via `modify` only | | `use_count` | No | Yes | | Equality short-circuit |
No | Yes (identical_to) | | Const propagation | Yes | Yes (always const) | |
Allocator-aware | Yes | Yes |

Prefer `indirect<T>` when mutation is frequent and sharing is not expected.
Prefer `copy_on_write<T>` when copies are frequent, mutations are rare, or
sharing can be exploited for efficiency.

# Reference Implementation

A C++20 reference implementation is available at
https://github.com/purpleKarrot/copy-on-write.

# Acknowledgements

The authors would like to thank the authors of `stlab::copy_on_write` and Qt's
`QSharedDataPointer` for pioneering this pattern. Thanks also to the authors of
P3019 (`indirect` and `polymorphic`) for establishing the vocabulary type design
conventions followed here.

# References

_Qt QSharedDataPointer_ https://doc.qt.io/qt-6/qshareddatapointer.html

_`indirect` and `polymorphic`: Vocabulary Types for Composite Class Design_ J.
B. Coe, A. Peacock, S. Parent, 2025
https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p3019r14.html

_stlab copy_on_write_ https://stlab.cc/libraries/copy_on_write.hpp/

_MISRA Language Guidelines_ https://ldra.com/misra/

_High Integrity C++_
https://www.perforce.com/resources/qac/high-integrity-cpp-coding-standard

# Appendix A: Before and After Examples

We show how `copy_on_write` simplifies composite class design and reduces copy
overhead in a text-editor document model.

## Without `copy_on_write`

```cpp
// document.h

class document {
  std::vector<std::string> _lines;
public:
  document() = default;

  // Deep copy required — O(number of lines).
  document(const document&) = default;
  document& operator=(const document&) = default;

  document(document&&) noexcept = default;
  document& operator=(document&&) noexcept = default;

  void append(std::string line) {
    _lines.push_back(std::move(line));
  }

  void insert(std::size_t pos, std::string line) {
    _lines.insert(_lines.begin() + pos, std::move(line));
  }

  void erase(std::size_t pos) {
    _lines.erase(_lines.begin() + pos);
  }

  std::size_t line_count() const { return _lines.size(); }

  const std::string& line(std::size_t i) const {
    return _lines[i];
  }
};

// Storing undo history copies all lines on every edit.
class editor {
  std::vector<document> _history;
  std::size_t _current = 0;
public:
  void record() {
    // Deep copy of entire document.
    _history.push_back(_history[_current]);
    ++_current;
  }
};
```

## After, using `copy_on_write`

```cpp
// document.h

class document {
  std::copy_on_write<std::vector<std::string>> _lines;
public:
  document() = default;

  // Copy is O(1): only an atomic reference-count increment.
  document(const document&) noexcept = default;
  document& operator=(const document&) noexcept = default;

  document(document&&) noexcept = default;
  document& operator=(document&&) noexcept = default;

  void append(std::string line) {
    _lines.modify(
      [&](std::vector<std::string>& v) {
        v.push_back(std::move(line));
      });
  }

  void insert(std::size_t pos, std::string line) {
    _lines.modify(
      // In-place: exclusive ownership.
      [&](std::vector<std::string>& v) {
        v.insert(v.begin() + pos, line);
      },
      // Constructive: shared ownership, no prior copy.
      [&](const std::vector<std::string>& v) {
        std::vector<std::string> w;
        w.reserve(v.size() + 1);
        w.insert(w.end(), v.begin(), v.begin() + pos);
        w.push_back(std::move(line));
        w.insert(w.end(), v.begin() + pos, v.end());
        return w;
      });
  }

  void erase(std::size_t pos) {
    _lines.modify(
      [&](std::vector<std::string>& v) {
        v.erase(v.begin() + pos);
      },
      [&](const std::vector<std::string>& v) {
        std::vector<std::string> w;
        w.reserve(v.size() - 1);
        w.insert(w.end(), v.begin(), v.begin() + pos);
        w.insert(w.end(), v.begin() + pos + 1, v.end());
        return w;
      });
  }

  std::size_t line_count() const { return _lines->size(); }

  const std::string& line(std::size_t i) const {
    return (*_lines)[i];
  }

  // Two documents share data iff they are identical.
  bool shares_data_with(const document& other) const {
    return _lines.identical_to(other._lines);
  }
};

// Storing undo history is now O(1) per entry.
class editor {
  std::vector<document> _history;
  std::size_t _current = 0;
public:
  void record() {
    // O(1) copy: reference count increment only.
    _history.push_back(_history[_current]);
    ++_current;
  }
};
```

The `insert` and `erase` operations demonstrate the two-argument `modify`
overload: when the document's lines are exclusively owned, the in-place
algorithm is used; when the lines are shared (e.g., between two undo states),
the constructive algorithm builds a new vector from scratch, avoiding a
redundant copy followed by a mutation.

The `shares_data_with` method demonstrates `identical_to`. Two undo states that
have not yet diverged will return `true`, allowing an editor to skip redundant
display refreshes or diffs.
