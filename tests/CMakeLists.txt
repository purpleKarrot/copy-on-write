cmake_minimum_required(VERSION 4.0)

# doctest 2.4.11 declares cmake_minimum_required(VERSION 2.8), which CMake 4.0
# no longer accepts without this policy variable.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

include(FetchContent)

FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.11
)
FetchContent_MakeAvailable(doctest)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)

function(add_doctest_executable target)
  add_executable(${target} ${ARGN})
  target_link_libraries(${target} PRIVATE copy_on_write doctest::doctest)
  doctest_discover_tests(${target})
endfunction()

add_doctest_executable(test_construction test_construction.cpp)
add_doctest_executable(test_observers    test_observers.cpp)
add_doctest_executable(test_modifiers    test_modifiers.cpp)
add_doctest_executable(test_assignment   test_assignment.cpp)
add_doctest_executable(test_comparison   test_comparison.cpp)
add_doctest_executable(test_hash         test_hash.cpp)
add_doctest_executable(test_allocator    test_allocator.cpp)
