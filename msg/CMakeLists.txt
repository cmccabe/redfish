# libutil needs to use -fPIC so that libutil can be used in the client shared libraries.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

add_library(msgr
    bsend.c
    fast_log.c
    msg.c
    msgr.c
    recv_pool.c
    xdr.c
)
target_link_libraries(msgr
    common
    types
    util
    ${LIBEV_LIBRARIES}
)

add_executable(msgr_unit msgr_unit.c)
target_link_libraries(msgr_unit core msgr utest)
add_utest(msgr_unit)

add_executable(bsend_unit bsend_unit.c)
target_link_libraries(bsend_unit core msgr utest)
add_utest(bsend_unit)

add_executable(recv_pool_unit recv_pool_unit.c)
target_link_libraries(recv_pool_unit core msgr utest)
add_utest(recv_pool_unit)

ADD_CUSTOM_COMMAND(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/types.c
    COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR} && rpcgen -c types.x > ${CMAKE_CURRENT_BINARY_DIR}/types.c
    DEPENDS types.x)

ADD_CUSTOM_COMMAND(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/types.h
    COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR} && rpcgen -h types.x > ${CMAKE_CURRENT_BINARY_DIR}/types.h
    DEPENDS types.x)

add_library(types
    ${CMAKE_CURRENT_BINARY_DIR}/types.c
    ${CMAKE_CURRENT_BINARY_DIR}/types.h
)
# Don't complain about the generated .c files
set_target_properties(types
    PROPERTIES COMPILE_FLAGS "-Wno-all -Wno-error")
