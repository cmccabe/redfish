# libutil needs to use -fPIC so that libutil can be used in the client shared libraries.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

add_library(util
    circ_compare.c
    config.c
    cram.c
    dir.c
    fast_log.c
    fast_log_mgr.c
    fast_log_types.c
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
    time.c
    username.c
)
target_link_libraries(util ${JSON_C_LIBRARY} platform pthread)

IF (CMAKE_SYSTEM MATCHES "Linux")
    FIND_LIBRARY(RT_LIBRARY rt)
    target_link_libraries(util rt)
ENDIF (CMAKE_SYSTEM MATCHES "Linux")

add_library(utest
    test.c
)
target_link_libraries(utest pthread)

add_subdirectory(platform)

add_executable(fast_log_unit fast_log_unit.c)
target_link_libraries(fast_log_unit core utest)
add_utest(fast_log_unit)

add_executable(tempfile_unit tempfile_unit.c)
target_link_libraries(tempfile_unit util utest)
add_utest(tempfile_unit)

add_executable(run_cmd_unit run_cmd_unit.c)
target_link_libraries(run_cmd_unit util utest)
add_utest(run_cmd_unit)

add_executable(compiler_unit compiler_unit.c)
target_link_libraries(compiler_unit util utest)
add_utest(compiler_unit)

add_executable(util_config_unit config_unit.c)
target_link_libraries(util_config_unit util utest)
add_utest(util_config_unit)

add_executable(string_unit string_unit.c)
target_link_libraries(string_unit util utest)
add_utest(string_unit)

add_executable(run_cmd_stdin_test run_cmd_stdin_test.c)

add_executable(simple_io_unit simple_io_unit.c)
target_link_libraries(simple_io_unit util utest)
add_utest(simple_io_unit)

add_executable(dir_unit dir_unit.c)
target_link_libraries(dir_unit util utest)
add_utest(dir_unit)

add_executable(str_to_int_unit str_to_int_unit.c)
target_link_libraries(str_to_int_unit util utest)
add_utest(str_to_int_unit)

add_executable(macro_unit macro_unit.c)
target_link_libraries(macro_unit util utest)
add_utest(macro_unit)

add_executable(path_unit path_unit.c)
target_link_libraries(path_unit util utest)
add_utest(path_unit)

add_executable(data_struct_unit data_struct_unit.c)
target_link_libraries(data_struct_unit util utest)
add_utest(data_struct_unit)

add_executable(bitfield_unit bitfield_unit.c)
target_link_libraries(bitfield_unit util utest)
add_utest(bitfield_unit)

add_executable(packed_unit packed_unit.c)
target_link_libraries(packed_unit util utest)
add_utest(packed_unit)

add_executable(username_unit username_unit.c)
target_link_libraries(username_unit util utest)
add_utest(username_unit)

add_executable(time_unit time_unit.c)
target_link_libraries(time_unit util utest)
add_utest(time_unit)
