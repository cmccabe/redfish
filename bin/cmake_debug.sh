#!/bin/bash

SNAME="`readlink -f $0`"
DIRNAME="`dirname "$SNAME"`"

cmake \
-DCMAKE_BUILD_TYPE=Debug \
-DCTEST_MEMORYCHECK_COMMAND:FILEPATH=/usr/bin/valgrind \
-DCTEST_MEMORYCHECK_COMMAND_OPTIONS:STRING="--trace-children=yes \
--quiet --tool=memcheck --leak-check=yes --show-reachable=yes \
--num-callers=100 --verbose --demangle" \
${@} \
"${DIRNAME}/.."
