cmake_minimum_required(VERSION 4.0)

project(copy_on_write CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(copy_on_write INTERFACE)
target_include_directories(copy_on_write INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

enable_testing()
add_subdirectory(tests)
