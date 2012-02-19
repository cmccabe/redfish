add_library(core
    alarm.c
    env.c
    glitch_log.c
    pid_file.c
    process_ctx.c
    signal.c
)
target_link_libraries(core common jorm util pthread)

add_executable(glitch_log_unit glitch_log_unit.c)
target_link_libraries(glitch_log_unit core utest)
add_test(glitch_log_unit ${CMAKE_CURRENT_BINARY_DIR}/glitch_log_unit glitch_log_unit)

add_executable(signal_unit signal_unit.c)
target_link_libraries(signal_unit core utest)
add_test(signal_unit ${CMAKE_CURRENT_BINARY_DIR}/signal_unit signal_unit)

add_executable(alarm_unit alarm_unit.c)
target_link_libraries(alarm_unit core utest)
add_test(alarm_unit ${CMAKE_CURRENT_BINARY_DIR}/alarm_unit alarm_unit)
