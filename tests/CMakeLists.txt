# SPDX-License-Identifier: BSL-1.0

cmake_minimum_required(VERSION 3.31)

include(FetchContent)
FetchContent_Declare(googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        52eb8108c5bdec04579160ae17225d66034bd723 # release-1.17.0
  )
FetchContent_MakeAvailable(googletest)

add_executable(cow_tests
  construction.cpp
  observers.cpp
  modifiers.cpp
  assignment.cpp
  comparison.cpp
  hash.cpp
  allocator.cpp
  )
target_link_libraries(cow_tests PRIVATE copy_on_write GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(cow_tests
  TEST_PREFIX "CopyOnWrite."
  DISCOVERY_MODE PRE_TEST
  )
