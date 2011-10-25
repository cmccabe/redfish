# This little library should contain all the Linux-specific stuff in the
# project.

add_library(platform
    pipe2.c
    readdir.c
    socket.c
)
