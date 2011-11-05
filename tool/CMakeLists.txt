add_executable(fishtool
    chmod.c
    chown.c
    main.c
    mkdirs.c
    ping.c
    read.c
    write.c
)
target_link_libraries(fishtool ${REDFISH_CLIENT_LIB} core)
