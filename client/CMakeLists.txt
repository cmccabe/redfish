#
# Client library
#
# There are three client libraries which all implement the fishc.h API.
#
# 1. A stub one, which actually just uses the local filesystem and extended
# attributes.
# 2. An HDFS one, which forwards all calls to HDFS
# 3. A real one which talks to the OneFish metadata and object servers.
#
# The third one is intended for production use; the first two are only for
# testing. We'll always build all three libraries, but we'll only install
# one of them as libfishc.
#

add_library(fishc_stub SHARED
    mloc.c
    stub/stub.c
    stub/xattrs.c
)
target_link_libraries(fishc_stub util pthread)
IF(${ONEFISH_CLIENT_LIB} STREQUAL fishc_stub)
    set_target_properties(fishc_stub PROPERTIES OUTPUT_NAME fishc)
ENDIF(${ONEFISH_CLIENT_LIB} STREQUAL fishc_stub)

add_executable(xattrs_unit stub/xattrs.c stub/xattrs_unit.c)
target_link_libraries(xattrs_unit util utest)
add_test(xattrs_unit ${CMAKE_CURRENT_BINARY_DIR}/xattrs_unit xattrs_unit)

#add_library(fishc_hdfs SHARED
#    mloc.c
#    hdfs/...
#)
#target_link_libraries(fishc_hdfs util pthread)
#IF(${ONEFISH_CLIENT_LIB} STREQUAL fishc_hdfs)
#    set_target_properties(fishc_hdfs PROPERTIES OUTPUT_NAME fishc)
#ENDIF(${ONEFISH_CLIENT_LIB} STREQUAL fishc_hdfs)
#
#add_library(fishc_real SHARED
#    mloc.c
#    true/...
#)
#target_link_libraries(onefish util pthread)
#IF(${ONEFISH_CLIENT_LIB} STREQUAL fishc_real)
#    set_target_properties(fishc_real PROPERTIES OUTPUT_NAME fishc)
#ENDIF(${ONEFISH_CLIENT_LIB} STREQUAL fishc_real)

INSTALL(FILES fishc.h DESTINATION include)
INSTALL(TARGETS ${ONEFISH_CLIENT_LIB} LIBRARY DESTINATION lib)
