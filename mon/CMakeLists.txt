add_executable(../fishmon 
    act/ssh_test.c
    act/write_file_test.c
    action.c
    daemon_worker.c
    main.c
    mon_config.c
    mon_info.c
    output_worker.c
    ssh.c
    worker.c
)
target_link_libraries(../fishmon core jorm)

add_executable(../arg_parsing_unit arg_parsing_unit.c)
target_link_libraries(../arg_parsing_unit util utest)
add_test(arg_parsing_unit ${CMAKE_CURRENT_BINARY_DIR}/../arg_parsing_unit arg_parsing_unit)

add_executable(worker_unit worker.c worker_unit.c)
target_link_libraries(worker_unit util utest)
add_test(worker_unit ${CMAKE_CURRENT_BINARY_DIR}/worker_unit worker_unit)
