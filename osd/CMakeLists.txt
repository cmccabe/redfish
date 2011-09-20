add_library(io
    io.c
)
target_link_libraries(io util)

add_executable(../fishosd
    main.c
)
target_link_libraries(../fishosd
    core
    io
    jorm
)

add_executable(io_unit io_unit.c)
target_link_libraries(io_unit io utest)
add_test(io_unit ${CMAKE_CURRENT_BINARY_DIR}/io_unit io_unit)
