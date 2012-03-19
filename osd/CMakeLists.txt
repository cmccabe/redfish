add_executable(fishosd
    fast_log.c
    main.c
    net.c
    ostor.c
)
target_link_libraries(fishosd
    ${LIBEV_LIBRARIES}
    common 
    core
    jorm
    msgr
)

INSTALL(TARGETS fishosd DESTINATION bin)

add_executable(ostor_unit
    fast_log.c
    ostor.c
    ostor_unit.c
)
target_link_libraries(ostor_unit core utest)
add_utest(ostor_unit)
