#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <copy_on_write.hpp>

#include <memory>
#include <memory_resource>
#include <string>
#include <vector>

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
    using propagate_on_container_swap            = std::false_type;
    using is_always_equal                        = std::false_type;

    int* alloc_count   = nullptr;
    int* dealloc_count = nullptr;
    int  id            = 0;

    tracking_allocator() = default;

    tracking_allocator(int* ac, int* dc, int id_)
        : alloc_count(ac), dealloc_count(dc), id(id_)
    {}

    template <typename U>
    tracking_allocator(tracking_allocator<U> const& o) noexcept
        : alloc_count(o.alloc_count), dealloc_count(o.dealloc_count), id(o.id)
    {}

    T* allocate(std::size_t n)
    {
        if (alloc_count) ++(*alloc_count);
        return std::allocator<T>{}.allocate(n);
    }

    void deallocate(T* p, std::size_t n) noexcept
    {
        if (dealloc_count) ++(*dealloc_count);
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
    struct rebind { using other = pocca_allocator<U>; };

    using tracking_allocator<T>::tracking_allocator;

    template <typename U>
    pocca_allocator(pocca_allocator<U> const& o) noexcept
        : tracking_allocator<T>(o)
    {}
};

template <typename T>
struct pocma_allocator : tracking_allocator<T>
{
    using propagate_on_container_move_assignment = std::true_type;

    template <typename U>
    struct rebind { using other = pocma_allocator<U>; };

    using tracking_allocator<T>::tracking_allocator;

    template <typename U>
    pocma_allocator(pocma_allocator<U> const& o) noexcept
        : tracking_allocator<T>(o)
    {}
};

template <typename T>
struct pocs_allocator : tracking_allocator<T>
{
    using propagate_on_container_swap = std::true_type;

    template <typename U>
    struct rebind { using other = pocs_allocator<U>; };

    using tracking_allocator<T>::tracking_allocator;

    template <typename U>
    pocs_allocator(pocs_allocator<U> const& o) noexcept
        : tracking_allocator<T>(o)
    {}
};

// ---------------------------------------------------------------------------
// Basic allocation tracking
// ---------------------------------------------------------------------------

TEST_CASE("exactly one allocation per constructed object")
{
    int allocs = 0, deallocs = 0;
    tracking_allocator<int> ta(&allocs, &deallocs, 1);
    {
        xyz::copy_on_write<int, tracking_allocator<int>> x(std::allocator_arg, ta, 42);
        CHECK(allocs == 1);
    }
    CHECK(deallocs == 1);
}

TEST_CASE("destructor deallocates when use_count drops to zero")
{
    int allocs = 0, deallocs = 0;
    tracking_allocator<int> ta(&allocs, &deallocs, 1);
    {
        xyz::copy_on_write<int, tracking_allocator<int>> a(std::allocator_arg, ta, 1);
        {
            xyz::copy_on_write<int, tracking_allocator<int>> b(a);
            CHECK(deallocs == 0);
        }
        CHECK(deallocs == 0); // a still alive
    }
    CHECK(deallocs == 1); // now freed
}

// ---------------------------------------------------------------------------
// Copy with same vs. different allocator
// ---------------------------------------------------------------------------

TEST_CASE("copy with same allocator shares the model (no extra allocation)")
{
    int allocs = 0, deallocs = 0;
    tracking_allocator<int> ta(&allocs, &deallocs, 1);
    xyz::copy_on_write<int, tracking_allocator<int>> a(std::allocator_arg, ta, 5);
    int allocs_before = allocs;
    xyz::copy_on_write<int, tracking_allocator<int>> b(std::allocator_arg, ta, a);
    CHECK(allocs == allocs_before); // no new allocation
    CHECK(a.identical_to(b));
}

TEST_CASE("copy with different allocator allocates a new model")
{
    int allocs1 = 0, deallocs1 = 0;
    int allocs2 = 0, deallocs2 = 0;
    tracking_allocator<int> ta1(&allocs1, &deallocs1, 1);
    tracking_allocator<int> ta2(&allocs2, &deallocs2, 2);

    xyz::copy_on_write<int, tracking_allocator<int>> a(std::allocator_arg, ta1, 5);
    xyz::copy_on_write<int, tracking_allocator<int>> b(std::allocator_arg, ta2, a);

    CHECK(allocs2 == 1);
    CHECK_FALSE(a.identical_to(b));
    CHECK(*b == 5);
}

// ---------------------------------------------------------------------------
// POCCA
// ---------------------------------------------------------------------------

TEST_CASE("POCCA: allocator is propagated on copy assignment")
{
    int allocs1 = 0, deallocs1 = 0;
    int allocs2 = 0, deallocs2 = 0;
    pocca_allocator<int> ta1(&allocs1, &deallocs1, 1);
    pocca_allocator<int> ta2(&allocs2, &deallocs2, 2);

    xyz::copy_on_write<int, pocca_allocator<int>> a(std::allocator_arg, ta1, 10);
    xyz::copy_on_write<int, pocca_allocator<int>> b(std::allocator_arg, ta2, 20);

    b = a;

    // After copy assignment with POCCA, b should use a's allocator.
    CHECK(b.get_allocator() == ta1);
    CHECK(*b == 10);
}

// ---------------------------------------------------------------------------
// POCMA
// ---------------------------------------------------------------------------

TEST_CASE("POCMA: allocator is propagated on move assignment")
{
    int allocs1 = 0, deallocs1 = 0;
    int allocs2 = 0, deallocs2 = 0;
    pocma_allocator<int> ta1(&allocs1, &deallocs1, 1);
    pocma_allocator<int> ta2(&allocs2, &deallocs2, 2);

    xyz::copy_on_write<int, pocma_allocator<int>> a(std::allocator_arg, ta1, 10);
    xyz::copy_on_write<int, pocma_allocator<int>> b(std::allocator_arg, ta2, 20);

    b = std::move(a);

    CHECK(b.get_allocator() == ta1);
    CHECK(*b == 10);
    CHECK(a.valueless_after_move());
}

TEST_CASE("without POCMA: move assignment with different allocators moves the value")
{
    int allocs1 = 0, deallocs1 = 0;
    int allocs2 = 0, deallocs2 = 0;
    tracking_allocator<int> ta1(&allocs1, &deallocs1, 1);
    tracking_allocator<int> ta2(&allocs2, &deallocs2, 2);

    xyz::copy_on_write<int, tracking_allocator<int>> a(std::allocator_arg, ta1, 55);
    xyz::copy_on_write<int, tracking_allocator<int>> b(std::allocator_arg, ta2, 0);

    b = std::move(a);

    CHECK(*b == 55);
    // b keeps its allocator (POCMA is false)
    CHECK(b.get_allocator() == ta2);
}

// ---------------------------------------------------------------------------
// POCS
// ---------------------------------------------------------------------------

TEST_CASE("POCS: allocators are swapped on member swap")
{
    int allocs1 = 0, deallocs1 = 0;
    int allocs2 = 0, deallocs2 = 0;
    pocs_allocator<int> ta1(&allocs1, &deallocs1, 1);
    pocs_allocator<int> ta2(&allocs2, &deallocs2, 2);

    xyz::copy_on_write<int, pocs_allocator<int>> a(std::allocator_arg, ta1, 111);
    xyz::copy_on_write<int, pocs_allocator<int>> b(std::allocator_arg, ta2, 222);

    a.swap(b);

    CHECK(*a == 222);
    CHECK(*b == 111);
    CHECK(a.get_allocator() == ta2);
    CHECK(b.get_allocator() == ta1);
}

// ---------------------------------------------------------------------------
// xyz::pmr::copy_on_write
// ---------------------------------------------------------------------------

TEST_CASE("pmr::copy_on_write works with monotonic_buffer_resource")
{
    std::array<std::byte, 1024> buf;
    std::pmr::monotonic_buffer_resource pool(buf.data(), buf.size());
    std::pmr::polymorphic_allocator<int> pmr_alloc(&pool);

    xyz::pmr::copy_on_write<int> x(std::allocator_arg, pmr_alloc, 42);
    CHECK(*x == 42);
    CHECK_FALSE(x.valueless_after_move());
}

TEST_CASE("pmr::copy_on_write copy shares the model")
{
    std::array<std::byte, 1024> buf;
    std::pmr::monotonic_buffer_resource pool(buf.data(), buf.size());
    std::pmr::polymorphic_allocator<int> pmr_alloc(&pool);

    xyz::pmr::copy_on_write<int> a(std::allocator_arg, pmr_alloc, 7);
    xyz::pmr::copy_on_write<int> b(std::allocator_arg, pmr_alloc, a);

    CHECK(a.identical_to(b));
    CHECK(*b == 7);
}
