# libutil needs to use -fPIC so that libutil can be used in the client shared libraries.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

include_directories(${JNI_INCLUDE_DIRS})

add_library(hfishc SHARED
    RedfishClient.c
    RedfishDataInputStream.c
    RedfishDataOutputStream.c
    common.c
)
target_link_libraries(hfishc ${JAVA_JVM_LIBRARY} pthread)
