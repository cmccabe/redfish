add_library(rsem
    rsem.c
)
target_link_libraries(rsem core jorm pthread util)

add_library(rsem_cli
    rsem_cli.c
)
target_link_libraries(rsem_cli rsem)

add_library(rsem_srv
    rsem_srv.c
)
target_link_libraries(rsem_srv rsem)

add_executable(rsem_unit rsem_unit.c)
target_link_libraries(rsem_unit rsem_srv rsem_cli utest)
add_test(rsem_unit ${CMAKE_CURRENT_BINARY_DIR}/rsem_unit rsem_unit)
