# libutil needs to use -fPIC so that libutil can be used in the client shared libraries.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

add_library(msgr
    fast_log.c
    msg.c
    msgr.c
)
target_link_libraries(msgr
    common
    util
    ${LIBEV_LIBRARIES}
)

add_executable(msgr_unit msgr_unit.c)
target_link_libraries(msgr_unit core msgr utest)
add_test(msgr_unit ${CMAKE_CURRENT_BINARY_DIR}/msgr_unit msgr_unit)
