# SPDX-License-Identifier: BSL-1.0

cmake_minimum_required(VERSION 4.0)
project(CopyOnWrite)

include(CTest)

add_library(copy_on_write INTERFACE)

target_sources(copy_on_write INTERFACE FILE_SET HEADERS FILES copy_on_write.hpp)
target_compile_features(copy_on_write INTERFACE cxx_std_20)

add_subdirectory(tests)
