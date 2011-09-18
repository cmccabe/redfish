add_library(util
    config.c
    dir.c
    run_cmd.c
    safe_io.c
    simple_io.c
    string.c
    tempfile.c
)
target_link_libraries(util ${JSON_C_LIBRARY} platform pthread)

add_library(utest
    test.c
)
target_link_libraries(utest pthread)

add_subdirectory(platform)

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

add_executable(simple_io_unit config_unit.c)
target_link_libraries(simple_io_unit util utest)
add_test(simple_io_unit ${CMAKE_CURRENT_BINARY_DIR}/simple_io_unit simple_io_unit)

add_executable(dir_unit dir_unit.c)
target_link_libraries(dir_unit util utest)
add_test(dir_unit ${CMAKE_CURRENT_BINARY_DIR}/dir_unit dir_unit)
