add_library(jorm
    json.c
)
target_link_libraries(jorm ${JSON_C_LIBRARY} util)

add_executable(jorm_unit jorm_unit.c)
target_link_libraries(jorm_unit jorm util utest)
add_test(jorm_unit ${CMAKE_CURRENT_BINARY_DIR}/jorm_unit jorm_unit)

add_executable(json_unit json_unit.c)
target_link_libraries(json_unit jorm util utest)
add_test(json_unit ${CMAKE_CURRENT_BINARY_DIR}/json_unit json_unit)
