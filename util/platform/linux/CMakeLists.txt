# This little library should contain all the Linux-specific stuff in the
# project.

add_library(platform
    epoll.c
    pipe2.c
)
