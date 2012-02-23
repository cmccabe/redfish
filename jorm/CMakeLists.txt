add_library(jorm
    json.c
)
target_link_libraries(jorm ${JSON_C_LIBRARY} util)

add_executable(jorm_unit jorm_unit.c)
target_link_libraries(jorm_unit jorm util utest)
add_utest(jorm_unit)

add_executable(json_unit json_unit.c)
target_link_libraries(json_unit jorm util utest)
add_utest(json_unit)
