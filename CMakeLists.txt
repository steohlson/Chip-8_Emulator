cmake_minimum_required(VERSION 3.10.0)
project(Chip-8 VERSION 0.1.0 LANGUAGES C)

add_subdirectory(src)

include(CTest)
enable_testing()

