cmake_minimum_required(VERSION 3.28...3.31)

project(containers C)

add_library(
    containers STATIC
    src/vector.c
    src/forward_list.c
    src/list.c
    src/binary_tree.c
    src/hash_map.c
    src/bit_set.c
    src/str.c
    src/priority_queue.c
)
target_include_directories(containers PUBLIC include)
