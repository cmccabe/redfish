add_library(core
    daemon.c
    daemon_ctx.c
    glitch_log.c
    log_config.c
    pid_file.c
    signal.c
)
target_link_libraries(core jorm util pthread)

add_executable(glitch_log_unit glitch_log_unit.c)
target_link_libraries(glitch_log_unit core utest)
add_test(glitch_log_unit ${CMAKE_CURRENT_BINARY_DIR}/glitch_log_unit glitch_log_unit)

add_executable(signal_unit signal_unit.c)
target_link_libraries(signal_unit core utest)
add_test(signal_unit ${CMAKE_CURRENT_BINARY_DIR}/signal_unit signal_unit)
