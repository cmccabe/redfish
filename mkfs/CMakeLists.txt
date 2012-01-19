add_executable(fishmkfs
    main.c
)
target_link_libraries(fishmkfs ${REDFISH_CLIENT_LIB} core)
