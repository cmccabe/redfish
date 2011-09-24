#add_library(fishc_stub SHARED
#    stub/stub.c
#)
#target_link_libraries(fishc_stub util pthread)

add_executable(xattrs_unit stub/xattrs.c stub/xattrs_unit.c)
target_link_libraries(xattrs_unit util utest)
add_test(xattrs_unit ${CMAKE_CURRENT_BINARY_DIR}/xattrs_unit xattrs_unit)

#add_library(fishc_hdfs SHARED
#    hdfs/...
#)
#target_link_libraries(fishc_hdfs util pthread)
#
#add_library(fishc SHARED
#    true/...
#)
#target_link_libraries(onefish util pthread)
