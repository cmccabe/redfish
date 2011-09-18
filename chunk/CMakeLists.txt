add_library(chunk
    io.c
)
target_link_libraries(chunk util pthread)

add_executable(io_unit io_unit.c)
target_link_libraries(io_unit chunk utest)
add_test(io_unit ${CMAKE_CURRENT_BINARY_DIR}/io_unit io_unit)
