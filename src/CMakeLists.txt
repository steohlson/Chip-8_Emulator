add_executable(${PROJECT_NAME} main.c chip8.c)
target_link_libraries(${PROJECT_NAME} PRIVATE Vendor)