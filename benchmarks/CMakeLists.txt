# SPDX-License-Identifier: BSL-1.0

add_executable(cow_mutation_benchmark mutation.cpp)
target_link_libraries(cow_mutation_benchmark PRIVATE copy_on_write)
