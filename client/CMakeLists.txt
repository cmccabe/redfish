#
# Client library
#
# There are three client libraries which all implement the fishc.h API.
#
# 1. A stub one, which actually just uses the local filesystem and extended
# attributes.
# 2. An HDFS one, which forwards all calls to HDFS
# 3. A real one which talks to the RedFish metadata and object servers.
#
# The third one is intended for production use; the first two are only for
# testing. We'll always build all three libraries, but we'll only install
# one of them as libfishc.
#

add_library(fishc_stub SHARED
    mloc.c
    stat.c
    stub/stub.c
    stub/xattrs.c
    version.c
)
target_link_libraries(fishc_stub common util pthread)
IF(${REDFISH_CLIENT_LIB} STREQUAL fishc_stub)
    set_target_properties(fishc_stub PROPERTIES OUTPUT_NAME fishc)
ENDIF(${REDFISH_CLIENT_LIB} STREQUAL fishc_stub)

add_executable(xattrs_unit stub/xattrs.c stub/xattrs_unit.c)
target_link_libraries(xattrs_unit common util utest)
add_test(xattrs_unit ${CMAKE_CURRENT_BINARY_DIR}/xattrs_unit xattrs_unit)

add_executable(version_unit version_unit.c version.c)
target_link_libraries(version_unit common util utest)
add_test(version_unit ${CMAKE_CURRENT_BINARY_DIR}/version_unit xattrs_unit)

#add_library(fishc_hdfs SHARED
#    hdfs/...
#    mloc.c
#    stat.c
#    version.c
#)
#target_link_libraries(fishc_hdfs common util pthread)
#IF(${REDFISH_CLIENT_LIB} STREQUAL fishc_hdfs)
#    set_target_properties(fishc_hdfs PROPERTIES OUTPUT_NAME fishc)
#ENDIF(${REDFISH_CLIENT_LIB} STREQUAL fishc_hdfs)
#
#add_library(fishc_real SHARED
#    mloc.c
#    stat.c
#    true/...
#    version.c
#)
#target_link_libraries(fishc_real common util pthread)
#IF(${REDFISH_CLIENT_LIB} STREQUAL fishc_real)
#    set_target_properties(fishc_real PROPERTIES OUTPUT_NAME fishc)
#ENDIF(${REDFISH_CLIENT_LIB} STREQUAL fishc_real)

INSTALL(FILES fishc.h DESTINATION include)
INSTALL(TARGETS ${REDFISH_CLIENT_LIB} LIBRARY DESTINATION lib)
