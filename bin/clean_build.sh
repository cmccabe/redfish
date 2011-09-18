#!/bin/sh

#
# clean_build.sh
#
# When run from a git repository containing a CMake project, does a clean build
# of that project in a temporary directory. Then runs make test.
#

die() {
    echo "CLEAN BUILD FAILURE!"
    echo $@
    exit 1
}

[ -d ./.git ] || die "must be run from a git repository"

CURPWD=`pwd`
CURGIT=`readlink -f "${CURPWD}"`
[ -d "${CURGIT}" ] || die "readlink -f failed?"
TMPDIR=`mktemp -d -t clean_build.XXXXXXXXXX` || exit 1
trap "rm -rf ${TMPDIR}; exit" INT TERM EXIT

cd "${TMPDIR}" || die "failed to cd to ${TMPDIR}"
git clone -- "${CURGIT}" git || die "failed to git clone ${REMOTE}"
cd git || die "failed to cd to git"
mkdir build || die "failed to mkdir build"
cd build || die "failed to cd to build"
cmake .. || die "cmake failed!"
make || die "make failed"
make test || die "make test failed"
echo "**** CLEAN BUILD SUCCESS ***"

exit 0
