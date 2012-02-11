add_executable(fishfuse
    fuse.c
)
target_link_libraries(fishfuse
    ${FUSE_C_LIBRARY}
    ${REDFISH_CLIENT_LIB}
    core
)
