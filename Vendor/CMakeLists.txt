set(SDL_TEST, OFF)
add_subdirectory(SDL3)

add_library(Vendor INTERFACE)
target_link_libraries(Vendor INTERFACE SDL3::SDL3)