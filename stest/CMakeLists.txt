add_library(stest stest.c)
target_link_libraries(stest core ${REDFISH_CLIENT_LIB} util)

add_executable(st_trivial trivial.c)
target_link_libraries(st_trivial stest)
install_stest(st_trivial)

add_executable(st_startup startup.c)
target_link_libraries(st_startup stest)
install_stest(st_startup)

add_executable(st_write_then_read write_then_read.c)
target_link_libraries(st_write_then_read ${REDFISH_CLIENT_LIB} stest)
install_stest(st_write_then_read)

add_executable(st_mkdirs mkdirs_test.c)
target_link_libraries(st_mkdirs ${REDFISH_CLIENT_LIB} stest)
install_stest(st_mkdirs)

add_executable(st_unlink unlink_test.c)
target_link_libraries(st_unlink ${REDFISH_CLIENT_LIB} stest)
install_stest(st_unlink)

add_executable(st_rename1 rename_test1.c)
target_link_libraries(st_rename1 ${REDFISH_CLIENT_LIB} stest)
install_stest(st_rename1)

add_executable(trivial_unit trivial_unit.c)
target_link_libraries(trivial_unit util utest)
add_utest(trivial_unit)

add_executable(st_fuse1 fuse1.c)
target_link_libraries(st_fuse1 stest)
install_stest(st_fuse1)

add_executable(posix_rename_test posix_rename_test.c)
target_link_libraries(posix_rename_test core utest)
