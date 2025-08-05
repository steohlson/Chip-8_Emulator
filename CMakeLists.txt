cmake_minimum_required(VERSION 3.10.0)
project(Chip-8 VERSION 0.1.0 LANGUAGES C)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(Vendor)
add_subdirectory(src)

include(CTest)
enable_testing()

