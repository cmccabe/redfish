# All of the operating system-specific stuff should go here.
# That means Linuxisms like epoll, etc.

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}")

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    add_subdirectory(linux)
else()
    add_subdirectory(posix)
endif()

add_executable(readdir_unit readdir_unit.c)
target_link_libraries(readdir_unit util utest)
add_test(readdir_unit ${CMAKE_CURRENT_BINARY_DIR}/readdir_unit readdir_unit)
