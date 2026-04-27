#include <gtest/gtest.h>

#include <copy_on_write.hpp>

#include <string>

// ---------------------------------------------------------------------------
// Copy assignment
// ---------------------------------------------------------------------------

TEST(Assignment, CopyAssignmentSetsValue)
{
    xyz::copy_on_write<int> a(1);
    xyz::copy_on_write<int> b(2);
    b = a;
    EXPECT_EQ(*b, 1);
}

TEST(Assignment, CopyAssignmentSharesOwnership)
{
    xyz::copy_on_write<int> a(1);
    xyz::copy_on_write<int> b(2);
    b = a;
    EXPECT_EQ(a.use_count(), 2);
    EXPECT_EQ(b.use_count(), 2);
    EXPECT_TRUE(a.identical_to(b));
}

TEST(Assignment, CopySelfAssignmentIsNoOp)
{
    xyz::copy_on_write<int> a(7);
    const int* ptr = &(*a);
    a = a; // NOLINT
    EXPECT_EQ(*a, 7);
    EXPECT_EQ(&(*a), ptr);
    EXPECT_EQ(a.use_count(), 1);
}

TEST(Assignment, CopyAssignmentReleasesOldValue)
{
    xyz::copy_on_write<int> a(99);
    xyz::copy_on_write<int> b(0);
    b = a;
    // b no longer solely owns its old model; a and b share a's model
    EXPECT_EQ(a.use_count(), 2);
}

TEST(Assignment, CopyAssignmentFromValuelessMakesTargetValueless)
{
    xyz::copy_on_write<int> a(1);
    xyz::copy_on_write<int> b(std::move(a)); // a is now valueless
    xyz::copy_on_write<int> c(5);
    c = a;
    EXPECT_TRUE(c.valueless_after_move());
}

// ---------------------------------------------------------------------------
// Move assignment
// ---------------------------------------------------------------------------

TEST(Assignment, MoveAssignmentTransfersValue)
{
    xyz::copy_on_write<int> a(10);
    xyz::copy_on_write<int> b(0);
    b = std::move(a);
    EXPECT_TRUE(a.valueless_after_move());
    EXPECT_EQ(*b, 10);
}

TEST(Assignment, MoveSelfAssignmentIsNoOp)
{
    xyz::copy_on_write<int> a(3);
    // Suppress the self-move warning; the class must handle it gracefully.
    auto& ref = a;
    a = std::move(ref);
    EXPECT_EQ(*a, 3);
}

TEST(Assignment, MoveAssignmentUseCountStaysOneOnTarget)
{
    xyz::copy_on_write<int> a(4);
    xyz::copy_on_write<int> b(0);
    b = std::move(a);
    EXPECT_EQ(b.use_count(), 1);
}

TEST(Assignment, MoveAssignmentFromValuelessMakesTargetValueless)
{
    xyz::copy_on_write<int> a(1);
    xyz::copy_on_write<int> b(std::move(a)); // a valueless
    xyz::copy_on_write<int> c(5);
    c = std::move(a);
    EXPECT_TRUE(c.valueless_after_move());
}

// ---------------------------------------------------------------------------
// Value assignment (operator=(U&&))
// ---------------------------------------------------------------------------

TEST(Assignment, ValueAssignmentUpdatesInPlaceWhenUnshared)
{
    xyz::copy_on_write<int> x(1);
    const int* ptr = &(*x);
    x = 42;
    EXPECT_EQ(*x, 42);
    // With use_count == 1 the implementation mutates in place
    EXPECT_EQ(&(*x), ptr);
}

TEST(Assignment, ValueAssignmentAllocatesNewModelWhenShared)
{
    xyz::copy_on_write<int> a(1);
    xyz::copy_on_write<int> b(a);
    b = 42;
    EXPECT_EQ(*b, 42);
    EXPECT_EQ(*a, 1);
    EXPECT_FALSE(a.identical_to(b));
    EXPECT_EQ(a.use_count(), 1);
    EXPECT_EQ(b.use_count(), 1);
}

TEST(Assignment, ValueAssignmentWithRvalueReference)
{
    xyz::copy_on_write<std::string> x("old");
    x = std::string("new");
    EXPECT_EQ(*x, "new");
}

