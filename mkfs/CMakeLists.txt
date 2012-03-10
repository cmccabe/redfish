IF (${REDFISH_CLIENT_LIB} STREQUAL fishc_stub)
    add_executable(fishmkfs
        mkfs.c
        stub.c
    )
    target_link_libraries(fishmkfs ${REDFISH_CLIENT_LIB} core)
ELSE (${REDFISH_CLIENT_LIB} STREQUAL fishc_stub)
    add_executable(fishmkfs
        mkfs.c
        real.c
    )
ENDIF (${REDFISH_CLIENT_LIB} STREQUAL fishc_stub)

INSTALL(TARGETS fishmkfs DESTINATION bin)
