add_library(stest stest.c)
target_link_libraries(stest core ${REDFISH_CLIENT_LIB} util)

add_executable(st_trivial trivial.c)
target_link_libraries(st_trivial stest)

add_executable(st_startup startup.c)
target_link_libraries(st_startup stest)

add_executable(st_write_then_read write_then_read.c)
target_link_libraries(st_write_then_read ${REDFISH_CLIENT_LIB} stest)

add_executable(trivial_unit trivial_unit.c)
target_link_libraries(trivial_unit util utest)
add_test(trivial_unit ${CMAKE_CURRENT_BINARY_DIR}/trivial_unit trivial_unit)

add_executable(posix_rename_test posix_rename_test.c)
target_link_libraries(posix_rename_test core utest)
