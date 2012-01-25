add_executable(fishmkfs
    mkfs.c
)
target_link_libraries(fishmkfs ${REDFISH_CLIENT_LIB} core)

INSTALL(TARGETS fishmkfs DESTINATION bin)
