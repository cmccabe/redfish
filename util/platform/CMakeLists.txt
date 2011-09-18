# All of the operating system-specific stuff should go here.
# That means Linuxisms like epoll, etc.

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    add_subdirectory(linux)
else()
    add_subdirectory(posix)
endif()
