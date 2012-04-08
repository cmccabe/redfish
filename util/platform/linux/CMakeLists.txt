# This little library should contain all the Linux-specific stuff in the
# project.

add_library(platform_linux
    pipe2.c
    readdir.c
    signal.c
    socket.c
    thread_id.c
)
