add_library(stest
    stest.c
)
target_link_libraries(stest core util)

add_executable(st_trivial
    trivial.c
)
target_link_libraries(st_trivial stest)

add_executable(st_startup
    startup.c
)
target_link_libraries(st_startup stest)

add_executable(trivial_unit trivial_unit.c)
target_link_libraries(trivial_unit util utest)
add_test(trivial_unit ${CMAKE_CURRENT_BINARY_DIR}/trivial_unit trivial_unit)
