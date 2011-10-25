# libutil needs to use -fPIC so that libutil can be used in the client shared libraries.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

add_library(msgr
    msg.c
    msgr.c
)
target_link_libraries(msgr
    core
    util
    ${LIBEV_LIBRARIES}
)

add_executable(msgr_unit msgr_unit.c)
target_link_libraries(msgr_unit msgr util utest)
add_test(msgr_unit ${CMAKE_CURRENT_BINARY_DIR}/msgr_unit msgr_unit)
