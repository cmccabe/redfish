add_library(stest
    stest.c
)
target_link_libraries(stest core util)

add_executable(st_trivial
    trivial.c
)
target_link_libraries(st_trivial stest)

#add_executable(worker_unit worker.c worker_unit.c)
#target_link_libraries(worker_unit util utest)
#add_test(worker_unit ${CMAKE_CURRENT_BINARY_DIR}/worker_unit worker_unit)
