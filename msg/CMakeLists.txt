# libutil needs to use -fPIC so that libutil can be used in the client shared libraries.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

add_library(msgr
    bsend.c
    fast_log.c
    msg.c
    msgr.c
    recv_pool.c
)
target_link_libraries(msgr
    common
    util
    ${LIBEV_LIBRARIES}
)

add_executable(msgr_unit msgr_unit.c)
target_link_libraries(msgr_unit core msgr utest)
add_test(msgr_unit ${CMAKE_CURRENT_BINARY_DIR}/msgr_unit msgr_unit)

add_executable(bsend_unit bsend_unit.c)
target_link_libraries(bsend_unit core msgr utest)
add_test(bsend_unit ${CMAKE_CURRENT_BINARY_DIR}/bsend_unit bsend_unit)

add_executable(recv_pool_unit recv_pool_unit.c)
target_link_libraries(recv_pool_unit core msgr utest)
add_test(recv_pool_unit ${CMAKE_CURRENT_BINARY_DIR}/recv_pool_unit recv_pool_unit)
