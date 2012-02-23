add_executable(fishmds
    delegation.c
    dmap.c
    dslots.c
    force_cpp.cc
    heartbeat.c
    main.c
    mstor.c
    net.c
    user.c
)
target_link_libraries(fishmds
    ${LEVELDB_LIBRARIES}
    ${LIBEV_LIBRARIES}
    core
    jorm
    msgr
)

add_executable(str_range_lock_unit str_range_lock_unit.c str_range_lock.c)
target_link_libraries(str_range_lock_unit util utest)
add_utest(str_range_lock_unit)

add_executable(leveldb_unit leveldb_unit.c force_cpp.cc)
target_link_libraries(leveldb_unit ${LEVELDB_LIBRARIES} util utest)
add_utest(leveldb_unit)

add_executable(mstor_unit mstor_unit.c mstor.c force_cpp.cc user.c)
target_link_libraries(mstor_unit core ${LEVELDB_LIBRARIES} util utest)
add_utest(mstor_unit)

add_executable(user_unit user_unit.c user.c)
target_link_libraries(user_unit core ${LEVELDB_LIBRARIES} util utest)
add_utest(user_unit)

add_executable(dslots_unit dslots_unit.c delegation.c dslots.c)
target_link_libraries(dslots_unit core util utest)
add_utest(dslots_unit)

add_executable(dmap_unit dmap_unit.c dmap.c)
target_link_libraries(dmap_unit core util utest)
add_utest(dmap_unit)

add_executable(fishmdump
    dump.c
    force_cpp.cc
    mstor.c
    user.c
)
target_link_libraries(fishmdump core ${LEVELDB_LIBRARIES} util)

INSTALL(TARGETS fishmds fishmdump DESTINATION bin)
