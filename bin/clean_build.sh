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

get_num_cpus() {
    ncpus=1
    if [ -e /proc/cpuinfo ]; then
        ncpus=`cat /proc/cpuinfo | grep '^processor' | wc -l`
    fi
}

[ -d ./.git ] || die "must be run from a git repository"

get_num_cpus
CURPWD=`pwd`
CURGIT=`readlink -f "${CURPWD}"`
[ -d "${CURGIT}" ] || die "readlink -f failed?"
TMPDIR=`mktemp -d -t clean_build.XXXXXXXXXX` || exit 1
trap "rm -rf ${TMPDIR}; exit" INT TERM EXIT

cd "${TMPDIR}" || die "failed to cd to ${TMPDIR}"
git clone -- "${CURGIT}" git || die "failed to git clone ${REMOTE}"
cd git || die "failed to cd to git"
for c in fishc_stub; do
    rm -rf build
    mkdir build || die "failed to mkdir build"
    cd build || die "failed to cd to build"
    cmake -DONEFISH_SO_REUSEADDR_HACK=1 \
          -DONEFISH_CLIENT_LIB=$c \
          .. || die "cmake failed!"
    make -j "${ncpus}" || die "make failed"
    make test || die "make test failed"
    cd ..
done
echo "**** CLEAN BUILD SUCCESS ***"

exit 0
