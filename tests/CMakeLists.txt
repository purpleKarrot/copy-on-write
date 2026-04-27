# SPDX-License-Identifier: BSL-1.0

cmake_minimum_required(VERSION 3.31)

find_package(GTest REQUIRED)

add_executable(cow_tests
  allocator.cpp
  assignment.cpp
  comparison.cpp
  composition.cpp
  composition_impl.cpp
  construction.cpp
  hash.cpp
  modifiers.cpp
  observers.cpp
  )
target_link_libraries(cow_tests PRIVATE copy_on_write GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(cow_tests
  TEST_PREFIX "CopyOnWrite."
  DISCOVERY_MODE PRE_TEST
  )
