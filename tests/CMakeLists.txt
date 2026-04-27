cmake_minimum_required(VERSION 4.0)

include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

function(add_gtest_executable target)
  add_executable(${target} ${ARGN})
  target_link_libraries(${target} PRIVATE copy_on_write GTest::gtest_main)
  gtest_discover_tests(${target})
endfunction()

add_gtest_executable(test_construction test_construction.cpp)
add_gtest_executable(test_observers    test_observers.cpp)
add_gtest_executable(test_modifiers    test_modifiers.cpp)
add_gtest_executable(test_assignment   test_assignment.cpp)
add_gtest_executable(test_comparison   test_comparison.cpp)
add_gtest_executable(test_hash         test_hash.cpp)
add_gtest_executable(test_allocator    test_allocator.cpp)
