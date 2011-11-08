add_executable(fishtool
    chmod.c
    chown.c
    locate.c
    main.c
    mkdirs.c
    ping.c
    read.c
    unlink.c
    write.c
)
target_link_libraries(fishtool ${REDFISH_CLIENT_LIB} core)
