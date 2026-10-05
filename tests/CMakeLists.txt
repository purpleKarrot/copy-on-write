# SPDX-License-Identifier: BSL-1.0

cmake_minimum_required(VERSION 3.31)

find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

add_executable(cow_tests
  allocator.cpp
  assignment.cpp
  comparison.cpp
  composition.cpp
  composition_impl.cpp
  construction.cpp
  constexpr.cpp
  hash.cpp
  incomplete_type.cpp
  incomplete_type_impl.cpp
  modifiers.cpp
  observers.cpp
  )

# Check the incomplete-type boundary without linking the out-of-line definitions.
# The baseline distinguishes the documented rejection from a broken fixture.
function(check_incomplete_type mode expected)
  set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
  set(standard 20)
  if(DEFINED CMAKE_CXX_STANDARD)
    set(standard "${CMAKE_CXX_STANDARD}")
  endif()
  set(definitions)
  if(NOT mode STREQUAL "OUT_OF_LINE")
    list(APPEND definitions "-DCOW_INLINE_${mode}")
  endif()
  try_compile(compiles
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/incomplete_type_compile.cpp"
    CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${PROJECT_SOURCE_DIR}"
    COMPILE_DEFINITIONS ${definitions}
    CXX_STANDARD ${standard}
    CXX_STANDARD_REQUIRED ON
    NO_CACHE
    OUTPUT_VARIABLE diagnostics
    )
  if(NOT "${compiles}" STREQUAL "${expected}")
    message(FATAL_ERROR "Incomplete-type check ${mode}: expected ${expected}, got ${compiles}\n${diagnostics}")
  endif()
endfunction()

check_incomplete_type(OUT_OF_LINE TRUE)
check_incomplete_type(MOVE_CTOR FALSE)
check_incomplete_type(MOVE_ASSIGN FALSE)
target_link_libraries(cow_tests PRIVATE copy_on_write GTest::gtest_main Threads::Threads)

include(GoogleTest)
gtest_discover_tests(cow_tests
  TEST_PREFIX "CopyOnWrite."
  DISCOVERY_MODE PRE_TEST
  )
