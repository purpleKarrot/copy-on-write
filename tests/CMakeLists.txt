cmake_minimum_required(VERSION 3.24)

include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
  URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926
)

# Prevent GoogleTest from overriding compiler/linker options
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

include(GoogleTest)

function(add_cow_test name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE GTest::gtest_main copy_on_write)
  gtest_discover_tests(${name})
endfunction()

add_cow_test(test_construction)
add_cow_test(test_observers)
add_cow_test(test_modifiers)
add_cow_test(test_assignment)
add_cow_test(test_comparison)
add_cow_test(test_hash)
add_cow_test(test_allocator)
