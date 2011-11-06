# This library contains the "generic" operating system support.  In general,
# this is not going to be as efficient as the Linux/BSD/whatever specific
# routines.

add_library(platform
    pipe2.c
    readdir.c
    socket.c
    thread_id.c
)
