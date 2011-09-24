cmake_minimum_required(VERSION 2.6.4 FATAL_ERROR)

# The name of our project is "OneFish". CMakeLists files in this project can
# refer to the root source directory of the project as ${ONEFISH_SOURCE_DIR} and
# to the root binary directory of the project as ${ONEFISH_BINARY_DIR}.
project(onefish)

enable_testing()

# Define "make check" as an alias for "make test."
add_custom_target(check COMMAND ctest)

set(CMAKE_BUILD_TYPE, Release) # can also be Debug
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -fno-strict-aliasing")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_REENTRANT -D_FILE_OFFSET_BITS=64")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE")
# Enable -rdynamic to get readable backtraces in the signal handlers.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -rdynamic")

set(CMAKE_C_FLAGS_RELEASE "-O3 -Wuninitialized")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -fstack-protector -Werror")
set(CMAKE_CURRENT_BINARY_DIR, builds)

# Find libjson-c
find_path(JSON_C_INCLUDE_DIR "json/json.h")
find_library(JSON_C_LIBRARY json)
IF(JSON_C_INCLUDE_DIR AND JSON_C_LIBRARY)
ELSE(JSON_C_INCLUDE_DIR AND JSON_C_LIBRARY)
    MESSAGE(FATAL_ERROR "Failed to find libjson-c. Try installing libjson-c with apt-get or yum, or install it manually from http://oss.metaparadigm.com/json-c/")
ENDIF(JSON_C_INCLUDE_DIR AND JSON_C_LIBRARY)

find_package(Curses REQUIRED)

# Set up include paths
include_directories(
    ${CURSES_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${JSON_C_INCLUDE_DIR}
)

add_subdirectory(client)
add_subdirectory(core)
add_subdirectory(jorm)
add_subdirectory(mds)
add_subdirectory(mon)
add_subdirectory(osd)
add_subdirectory(top)
add_subdirectory(util)
