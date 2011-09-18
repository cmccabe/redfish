add_library(core
    log_config.c
    glitch_log.c
    signal.c
)
target_link_libraries(core jorm util pthread)

add_executable(signal_unit signal_unit.c)
target_link_libraries(signal_unit core utest)
add_test(signal_unit ${CMAKE_CURRENT_BINARY_DIR}/signal_unit signal_unit)
