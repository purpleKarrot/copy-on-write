// SPDX-License-Identifier: BSL-1.0

#include <copy_on_write.hpp>
#include <gtest/gtest.h>

#include <atomic>
#include <functional>
#include <latch>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace {

struct tracked_payload
{
  std::atomic<int>* live;
  std::function<void()> on_copy;
  int value = 0;

  explicit tracked_payload(std::atomic<int>& count, std::function<void()> hook = {})
    : live{&count}
    , on_copy{std::move(hook)}
  {
    ++*live;
  }

  tracked_payload(tracked_payload const& other)
    : live{other.live}
    , on_copy{other.on_copy}
    , value{other.value}
  {
    ++*live;
    if (on_copy) {
      on_copy();
    }
  }

  tracked_payload(tracked_payload&& other) noexcept
    : live{other.live}
    , on_copy{std::move(other.on_copy)}
    , value{other.value}
  {
    ++*live;
  }

  ~tracked_payload() { --*live; }
};

} // namespace

TEST(Modifiers, DetachDestroysPayloadWhenOtherOwnerIsReleasedDuringConstruction)
{
  for (bool transform : {false, true}) {
    SCOPED_TRACE(transform ? "transform" : "action");
    std::atomic<int> live = 0;
    std::latch constructing{1}, owner_released{1};
    auto release_other_owner = [&] {
      constructing.count_down();
      owner_released.wait();
    };
    {
      xyz::copy_on_write<tracked_payload> value(std::in_place, live, release_other_owner);
      std::optional<xyz::copy_on_write<tracked_payload>> other(value);
      std::thread worker([&] {
        constructing.wait();
        other.reset();
        owner_released.count_down();
      });

      if (transform) {
        value.modify([](tracked_payload&) {},
                     [&](tracked_payload const&) {
                       release_other_owner();
                       return tracked_payload(live);
                     });
      } else {
        value.modify([](tracked_payload&) {});
      }
      worker.join();
      EXPECT_EQ(value.use_count(), 1);
      EXPECT_EQ(live.load(), 1);
    }
    EXPECT_EQ(live.load(), 0);
  }
}

// ---------------------------------------------------------------------------
// modify(action) — single-argument overload
// ---------------------------------------------------------------------------

TEST(Modifiers, ModifyActionMutatesInPlaceWhenUnshared)
{
  xyz::copy_on_write<int> x(5);
  int const* ptr_before = &(*x);
  x.modify([](int& v) { v += 10; });
  EXPECT_EQ(*x, 15);
  EXPECT_EQ(&(*x), ptr_before); // No new allocation: the pointer to the value should be unchanged.
}

TEST(Modifiers, ModifyActionDeepCopiesBeforeMutatingWhenShared)
{
  xyz::copy_on_write<int> a(5);
  xyz::copy_on_write<int> b(a);
  ASSERT_TRUE(a.identical_to(b));

  b.modify([](int& v) { v += 10; });

  EXPECT_EQ(*b, 15);
  EXPECT_EQ(*a, 5); // original unaffected
  EXPECT_FALSE(a.identical_to(b));
}

TEST(Modifiers, ModifyActionOnSharedLeavesOriginalUseCountAtOne)
{
  xyz::copy_on_write<int> a(1);
  xyz::copy_on_write<int> b(a);
  b.modify([](int& v) { v = 99; });
  EXPECT_EQ(a.use_count(), 1);
  EXPECT_EQ(b.use_count(), 1);
}

TEST(Modifiers, SharedActionFailurePreservesValueAndIdentityAndDestroysReplacement)
{
  std::atomic<int> live = 0;
  {
    xyz::copy_on_write<tracked_payload> original(std::in_place, live);
    auto value = original;
    auto address = &*value;

    EXPECT_THROW(value.modify([&](tracked_payload& replacement) {
      replacement.value = 99;
      EXPECT_EQ(live.load(), 2);
      EXPECT_EQ(&*value, address);
      EXPECT_EQ(value->value, 0);
      EXPECT_TRUE(value.identical_to(original));
      throw std::runtime_error("action failed");
    }),
                 std::runtime_error);

    EXPECT_EQ(value->value, 0);
    EXPECT_EQ(original->value, 0);
    EXPECT_EQ(&*value, address);
    EXPECT_TRUE(value.identical_to(original));
    EXPECT_EQ(value.use_count(), 2);
    EXPECT_EQ(live.load(), 1);
  }
  EXPECT_EQ(live.load(), 0);
}

TEST(Modifiers, SharedActionSuccessCommitsReplacementAfterCallback)
{
  xyz::copy_on_write<int> original(5);
  auto value = original;
  auto address = &*value;

  value.modify([&](int& replacement) {
    replacement = 15;
    EXPECT_EQ(&*value, address);
    EXPECT_EQ(*value, 5);
    EXPECT_TRUE(value.identical_to(original));
  });

  EXPECT_EQ(*value, 15);
  EXPECT_EQ(*original, 5);
  EXPECT_NE(&*value, address);
  EXPECT_EQ(value.use_count(), 1);
  EXPECT_EQ(original.use_count(), 1);
}

TEST(Modifiers, SharedActionFailureRetainsOriginalWhenOtherOwnerIsReleased)
{
  std::atomic<int> live = 0;
  {
    xyz::copy_on_write<tracked_payload> value(std::in_place, live);
    std::optional<xyz::copy_on_write<tracked_payload>> other(value);
    auto address = &*value;

    EXPECT_THROW(value.modify([&](tracked_payload& replacement) {
      replacement.value = 99;
      other.reset();
      EXPECT_EQ(live.load(), 2);
      throw std::runtime_error("action failed");
    }),
                 std::runtime_error);

    EXPECT_EQ(value->value, 0);
    EXPECT_EQ(&*value, address);
    EXPECT_EQ(value.use_count(), 1);
    EXPECT_EQ(live.load(), 1);
  }
  EXPECT_EQ(live.load(), 0);
}

TEST(Modifiers, ExclusiveActionFailureRetainsPartialMutation)
{
  for (bool transform : {false, true}) {
    SCOPED_TRACE(transform ? "transform" : "action");
    xyz::copy_on_write<int> value(5);
    auto address = &*value;
    auto action = [](int& v) {
      v = 15;
      throw std::runtime_error("action failed");
    };
    if (transform) {
      EXPECT_THROW(value.modify(action,
                                [](int const& v) {
                                  ADD_FAILURE() << "transformation invoked for exclusive ownership";
                                  return v;
                                }),
                   std::runtime_error);
    } else {
      EXPECT_THROW(value.modify(action), std::runtime_error);
    }
    EXPECT_EQ(*value, 15);
    EXPECT_EQ(&*value, address);
    EXPECT_EQ(value.use_count(), 1);
  }
}

TEST(Modifiers, SharedCopyFailurePreservesOwnershipAndDoesNotInvokeAction)
{
  struct throwing_copy
  {
    throwing_copy() = default;
    throwing_copy(throwing_copy const&) { throw std::runtime_error("copy failed"); }
  };
  xyz::copy_on_write<throwing_copy> original;
  auto value = original;
  auto address = &*value;
  bool action_called = false;

  EXPECT_THROW(value.modify([&](throwing_copy&) { action_called = true; }), std::runtime_error);

  EXPECT_FALSE(action_called);
  EXPECT_EQ(&*value, address);
  EXPECT_TRUE(value.identical_to(original));
  EXPECT_EQ(value.use_count(), 2);
}

// ---------------------------------------------------------------------------
// modify(action, transform) — two-argument overload
// ---------------------------------------------------------------------------

TEST(Modifiers, ModifyActionTransformCallsActionInPlaceWhenUnshared)
{
  xyz::copy_on_write<std::string> x("hello");
  bool action_called = false;
  bool transform_called = false;

  x.modify(
    [&](std::string& s) {
      action_called = true;
      s += "!";
    },
    [&](std::string const& s) -> std::string {
      transform_called = true;
      return s + "!";
    });

  EXPECT_EQ(*x, "hello!");
  EXPECT_TRUE(action_called);
  EXPECT_FALSE(transform_called);
}

TEST(Modifiers, ModifyActionTransformCallsTransformWhenShared)
{
  xyz::copy_on_write<std::string> a("hello");
  xyz::copy_on_write<std::string> b(a);
  bool action_called = false;
  bool transform_called = false;

  b.modify(
    [&](std::string& s) {
      action_called = true;
      s += "!";
    },
    [&](std::string const& s) -> std::string {
      transform_called = true;
      return s + "?";
    });

  EXPECT_EQ(*b, "hello?");
  EXPECT_EQ(*a, "hello"); // original unaffected
  EXPECT_TRUE(transform_called);
  EXPECT_FALSE(action_called);
}

TEST(Modifiers, SharedTransformationFailurePreservesValueAndIdentity)
{
  xyz::copy_on_write<int> original(5);
  auto value = original;
  auto address = &*value;
  bool action_called = false;

  EXPECT_THROW(
    value.modify([&](int&) { action_called = true; },
                 [](int const&) -> int { throw std::runtime_error("transformation failed"); }),
    std::runtime_error);

  EXPECT_FALSE(action_called);
  EXPECT_EQ(*value, 5);
  EXPECT_EQ(&*value, address);
  EXPECT_TRUE(value.identical_to(original));
  EXPECT_EQ(value.use_count(), 2);
}

TEST(Modifiers, ModifyInvokesMemberFunctionPointers)
{
  struct payload
  {
    int value;
    void increment() { ++value; }
    payload transformed() const { return {value + 1}; }
  };
  xyz::copy_on_write<payload> value(payload{5});
  value.modify(&payload::increment);
  EXPECT_EQ(value->value, 6);
  auto shared = value;
  shared.modify(&payload::increment);
  EXPECT_EQ(shared->value, 7);
  EXPECT_EQ(value->value, 6);
  shared = value;
  shared.modify(&payload::increment, &payload::transformed);
  EXPECT_EQ(shared->value, 7);
  shared.modify(&payload::increment, &payload::transformed);
  EXPECT_EQ(shared->value, 8);
  EXPECT_EQ(value->value, 6);
}

// ---------------------------------------------------------------------------
// swap (member)
// ---------------------------------------------------------------------------

TEST(Modifiers, MemberSwapExchangesValues)
{
  xyz::copy_on_write<int> a(1);
  xyz::copy_on_write<int> b(2);
  a.swap(b);
  EXPECT_EQ(*a, 2);
  EXPECT_EQ(*b, 1);
}

TEST(Modifiers, MemberSwapWithValuelessObject)
{
  xyz::copy_on_write<int> a(42);
  xyz::copy_on_write<int> b(std::move(a));
  // a is now valueless, b holds 42
  b.swap(a);
  EXPECT_EQ(*a, 42);
  EXPECT_TRUE(b.valueless_after_move());
}

// ---------------------------------------------------------------------------
// swap (free function)
// ---------------------------------------------------------------------------

TEST(Modifiers, FreeSwapDelegatesToMemberSwap)
{
  xyz::copy_on_write<int> a(10);
  xyz::copy_on_write<int> b(20);
  using std::swap;
  swap(a, b);
  EXPECT_EQ(*a, 20);
  EXPECT_EQ(*b, 10);
}
