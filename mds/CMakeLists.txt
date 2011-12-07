add_executable(fishmds
    force_cpp.cc
    main.c
    mstor.c
    net.c
)
target_link_libraries(fishmds
    ${LEVELDB_LIBRARIES}
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

add_executable(mstor_unit mstor_unit.c mstor.c force_cpp.cc)
target_link_libraries(mstor_unit core ${LEVELDB_LIBRARIES} util utest)
add_test(mstor_unit ${CMAKE_CURRENT_BINARY_DIR}/mstor_unit mstor_unit)

INSTALL(TARGETS fishmds DESTINATION bin)
