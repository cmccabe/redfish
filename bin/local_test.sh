#!/bin/bash

# Copyright 2012 the RedFish authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#
# local_test.sh
#
# Run some local tests with the stub client library.
# This should be run from the CMake build directory.
#

die() {
	echo -n "failed: "
	echo $@
	exit 1
}

export STUB_BASE=`mktemp -d -t local_test.XXXXXXXXXX`
trap "[ -n "${SKIP_CLEANUP}" ] || rm -rf ${STUB_BASE}; exit" INT TERM EXIT
echo "Tests running with STUB_BASE=${STUB_BASE}"

./mkfs/fishmkfs -c /tmp/conf -m 0 -f 1 || die "line ${LINENO}"
./stest/st_startup -f || die "line ${LINENO}"
./stest/st_trivial -f || die "line ${LINENO}"
./stest/st_write_then_read -f || die "line ${LINENO}"

echo "*** SUCCESS ***"
exit 0
