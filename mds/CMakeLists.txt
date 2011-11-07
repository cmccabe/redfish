add_executable(fishmds
    main.c
    net.c
)
target_link_libraries(fishmds
    ${LIBEV_LIBRARIES}
    core
    jorm
)

add_executable(str_range_lock_unit str_range_lock_unit.c str_range_lock.c)
target_link_libraries(str_range_lock_unit util utest)
add_test(str_range_lock_unit ${CMAKE_CURRENT_BINARY_DIR}/str_range_lock_unit str_range_lock_unit)

add_executable(leveldb_unit leveldb_unit.c force_cpp.cc)
target_link_libraries(leveldb_unit ${LEVELDB_LIBRARIES} util utest)
add_test(leveldb_unit ${CMAKE_CURRENT_BINARY_DIR}/leveldb_unit leveldb_unit)

INSTALL(TARGETS fishmds DESTINATION bin)
