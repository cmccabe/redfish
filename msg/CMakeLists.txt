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
    fish_msg_ty
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
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fish.c
    COMMAND rpcgen -c ${CMAKE_CURRENT_SOURCE_DIR}/fish.x > ${CMAKE_CURRENT_BINARY_DIR}/fish.c
    DEPENDS fish.x)

ADD_CUSTOM_COMMAND(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fish_internal.h
    COMMAND rpcgen -h ${CMAKE_CURRENT_SOURCE_DIR}/fish.x > ${CMAKE_CURRENT_BINARY_DIR}/fish_internal.h
    DEPENDS fish.x)

add_library(fish_msg_ty
    ${CMAKE_CURRENT_BINARY_DIR}/fish.c
    ${CMAKE_CURRENT_BINARY_DIR}/fish_internal.h
)
# Don't complain about the generated .c files
set_target_properties(fish_msg_ty
    PROPERTIES COMPILE_FLAGS "-Wno-all -Wno-error")
