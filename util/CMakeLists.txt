# libutil needs to use -fPIC so that libutil can be used in the client shared libraries.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

add_library(util
    config.c
    dir.c
    fast_log.c
    fast_log_mgr.c
    fast_log_types.c
    msleep.c
    net.c
    packed.c
    path.c
    run_cmd.c
    safe_io.c
    simple_io.c
    str_to_int.c
    string.c
    tempfile.c
    terror.c
    thread.c
    username.c
)
target_link_libraries(util ${JSON_C_LIBRARY} platform pthread)

add_library(utest
    test.c
)
target_link_libraries(utest pthread)

add_subdirectory(platform)

add_executable(fast_log_unit fast_log_unit.c)
target_link_libraries(fast_log_unit core utest)
add_test(fast_log_unit ${CMAKE_CURRENT_BINARY_DIR}/fast_log_unit fast_log_unit)

add_executable(tempfile_unit tempfile_unit.c)
target_link_libraries(tempfile_unit util utest)
add_test(tempfile_unit ${CMAKE_CURRENT_BINARY_DIR}/tempfile_unit tempfile_unit)

add_executable(run_cmd_unit run_cmd_unit.c)
target_link_libraries(run_cmd_unit util utest)
add_test(run_cmd_unit ${CMAKE_CURRENT_BINARY_DIR}/run_cmd_unit run_cmd_unit)

add_executable(compiler_unit compiler_unit.c)
target_link_libraries(compiler_unit util utest)
add_test(compiler_unit ${CMAKE_CURRENT_BINARY_DIR}/compiler_unit compiler_unit)

add_executable(util_config_unit config_unit.c)
target_link_libraries(util_config_unit util utest)
add_test(util_config_unit ${CMAKE_CURRENT_BINARY_DIR}/util_config_unit util_config_unit)

add_executable(string_unit string_unit.c)
target_link_libraries(string_unit util utest)
add_test(string_unit ${CMAKE_CURRENT_BINARY_DIR}/string_unit string_unit)

add_executable(run_cmd_stdin_test run_cmd_stdin_test.c)

add_executable(simple_io_unit simple_io_unit.c)
target_link_libraries(simple_io_unit util utest)
add_test(simple_io_unit ${CMAKE_CURRENT_BINARY_DIR}/simple_io_unit simple_io_unit)

add_executable(dir_unit dir_unit.c)
target_link_libraries(dir_unit util utest)
add_test(dir_unit ${CMAKE_CURRENT_BINARY_DIR}/dir_unit dir_unit)

add_executable(str_to_int_unit str_to_int_unit.c)
target_link_libraries(str_to_int_unit util utest)
add_test(str_to_int_unit ${CMAKE_CURRENT_BINARY_DIR}/str_to_int_unit str_to_int_unit)

add_executable(macro_unit macro_unit.c)
target_link_libraries(macro_unit util utest)
add_test(macro_unit ${CMAKE_CURRENT_BINARY_DIR}/macro_unit macro_unit)

add_executable(path_unit path_unit.c)
target_link_libraries(path_unit util utest)
add_test(path_unit ${CMAKE_CURRENT_BINARY_DIR}/path_unit path_unit)

add_executable(data_struct_unit data_struct_unit.c)
target_link_libraries(data_struct_unit util utest)
add_test(data_struct_unit ${CMAKE_CURRENT_BINARY_DIR}/data_struct_unit data_struct_unit)

add_executable(bitfield_unit bitfield_unit.c)
target_link_libraries(bitfield_unit util utest)
add_test(bitfield_unit ${CMAKE_CURRENT_BINARY_DIR}/bitfield_unit bitfield_unit)

add_executable(packed_unit packed_unit.c)
target_link_libraries(packed_unit util utest)
add_test(packed_unit ${CMAKE_CURRENT_BINARY_DIR}/packed_unit packed_unit)

add_executable(username_unit username_unit.c)
target_link_libraries(username_unit util utest)
add_test(username_unit ${CMAKE_CURRENT_BINARY_DIR}/username_unit username_unit)
