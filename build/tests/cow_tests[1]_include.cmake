if(EXISTS "/home/runner/work/copy-on-write/copy-on-write/build/tests/cow_tests")
  if(NOT EXISTS "/home/runner/work/copy-on-write/copy-on-write/build/tests/cow_tests[1]_tests.cmake" OR
     NOT "/home/runner/work/copy-on-write/copy-on-write/build/tests/cow_tests[1]_tests.cmake" IS_NEWER_THAN "/home/runner/work/copy-on-write/copy-on-write/build/tests/cow_tests" OR
     NOT "/home/runner/work/copy-on-write/copy-on-write/build/tests/cow_tests[1]_tests.cmake" IS_NEWER_THAN "${CMAKE_CURRENT_LIST_FILE}")
    include("/home/runner/.local/lib/python3.12/site-packages/cmake/data/share/cmake-4.3/Modules/GoogleTestAddTests.cmake")
    gtest_discover_tests_impl(
      TEST_EXECUTABLE [==[/home/runner/work/copy-on-write/copy-on-write/build/tests/cow_tests]==]
      TEST_EXECUTOR [==[]==]
      TEST_WORKING_DIR [==[/home/runner/work/copy-on-write/copy-on-write/build/tests]==]
      TEST_EXTRA_ARGS [==[]==]
      TEST_PROPERTIES [==[]==]
      TEST_PREFIX [==[CopyOnWrite.]==]
      TEST_SUFFIX [==[]==]
      TEST_FILTER [==[]==]
      NO_PRETTY_TYPES [==[FALSE]==]
      NO_PRETTY_VALUES [==[FALSE]==]
      TEST_LIST [==[cow_tests_TESTS]==]
      CTEST_FILE [==[/home/runner/work/copy-on-write/copy-on-write/build/tests/cow_tests[1]_tests.cmake]==]
      TEST_DISCOVERY_TIMEOUT [==[5]==]
      TEST_DISCOVERY_EXTRA_ARGS [==[]==]
      TEST_XML_OUTPUT_DIR [==[]==]
    )
  endif()
  include("/home/runner/work/copy-on-write/copy-on-write/build/tests/cow_tests[1]_tests.cmake")
else()
  add_test(cow_tests_NOT_BUILT cow_tests_NOT_BUILT)
endif()
