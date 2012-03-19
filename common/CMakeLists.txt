# libcommon needs to use -fPIC so that libutil can be used in the client shared libraries.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

add_library(common
    cluster_map.c
    entity_type.c
    config/logc.c
    config/mdsc.c
    config/mstorc.c
    config/osdc.c
    config/ostorc.c
    config/unitaryc.c
)
target_link_libraries(common jorm util pthread)

add_executable(cluster_map_unit cluster_map_unit.c)
target_link_libraries(cluster_map_unit common utest)
add_utest(cluster_map_unit)
