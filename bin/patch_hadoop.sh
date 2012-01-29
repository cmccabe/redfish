#!/bin/bash

# Copyright 2012 the Redfish authors
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
# patch_hadoop.sh
#
# Patches a Hadoop source tree for Redfish 
#

HADOOP_PATH=""
HADOOP_VERSION=""
VALID_HADOOP_VERSIONS="0.20 0.21"
PATCH_HADOOP_DIR="`dirname $0`"
REDFISH_SRC_BASE="${PATCH_HADOOP_DIR}/.."

die() {
    echo $@
    exit 1
}

usage() {
    cat <<EOF
patch_hadoop: patches a Hadoop source tree for Redfish 
-E <version>            The Hadoop version
-h                      This help message
-p <path>               The path to the Hadoop source
EOF
}

ensure_valid_version() {
    for ver in ${VALID_HADOOP_VERSIONS}; do
        [ "${ver}" == "${HADOOP_VERSION}" ] && return
    done
    die "Hadoop version \"${HADOOP_VERSION}\" is not recognized. \
Valid versions are: ${VALID_HADOOP_VERSIONS}"
}

get_core_default_path() {
    case "${HADOOP_VERSION}" in
        0.20) CORE_DEFAULT_PATH="src/core/core-default.xml";;
        0.21) CORE_DEFAULT_PATH="src/java/core-default.xml";;
        *) die "unknown hadoop version ${HADOOP_VERSION}"
    esac
}

get_fs_dir_path() {
    case "${HADOOP_VERSION}" in
        0.20) FS_DIR_PATH="src/core/org/apache/hadoop/fs/";;
        0.21) FS_DIR_PATH="src/java/org/apache/hadoop/fs/";;
        *) die "unknown hadoop version ${HADOOP_VERSION}"
    esac
}

get_fs_test_dir_path() {
    case "${HADOOP_VERSION}" in
        0.20) FS_TEST_DIR_PATH="src/test/org/apache/hadoop/fs/";;
        0.21) FS_TEST_DIR_PATH="src/test/core/org/apache/hadoop/fs/";;
        *) die "unknown hadoop version ${HADOOP_VERSION}"
    esac
}

while getopts  "E:hp:" flag; do
    case $flag in
        E)  HADOOP_VERSION=$OPTARG;;
        h) usage; exit 0;;
        p)  HADOOP_PATH=$OPTARG;;
        *) echo; usage; exit 1;;
    esac
done

# Validate inputs
[ -d "${REDFISH_SRC_BASE}" ] || die "Failed to stat Redfish source directory \
${REDFISH_SRC_BASE}"
[ "x${HADOOP_PATH}" != "x" ] || \
    die "You must specify a path to a Hadoop source tree.  -h for help."
[ -d "${HADOOP_PATH}" ] || die "Couldn't stat directory \"${HADOOP_PATH}\""
[ "x${HADOOP_VERSION}" != "x" ] || \
    die "You must specify a Hadoop version.  Valid versions are: \
${VALID_HADOOP_VERSIONS}. -h for help."
ensure_valid_version

# Patch core-default.xml
pushd "${HADOOP_PATH}" &> /dev/null || die "failed to cd to \"${HADOOP_PATH}\""
get_core_default_path
patch -p1 << EOF
--- a/${CORE_DEFAULT_PATH}  2012-01-28 11:44:26.729337749 -0800
+++ b/${CORE_DEFAULT_PATH}  2012-01-27 03:19:04.943228250 -0800
@@ -159,6 +159,12 @@
 </property>
 
 <property>
+  <name>fs.redfish.impl</name>
+  <value>org.apache.hadoop.fs.redfish.RedfishFileSystem</value>
+  <description>The FileSystem for redfish: uris.</description>
+</property>
+
+<property>
   <name>fs.kfs.impl</name>
   <value>org.apache.hadoop.fs.kfs.KosmosFileSystem</value>
   <description>The FileSystem for kfs: uris.</description>
EOF
[ $? -eq 0 ] || die "failed to patch core-default.xml"
popd &> /dev/null

# Make softlinks
# TODO: export files using C preprocessor instead, to support multiple versions
# via #defines
get_fs_dir_path
for f in RedfishFileSystem.java \
    RedfishDataOutputStream.java \
    RedfishDataInputStream.java \
    RedfishClient.java
do
    ln -s "${REDFISH_SRC_BASE}/hadoop/${f}" \
        "${HADOOP_PATH}/${FS_DIR_PATH}/${f}" \
            || die "failed to link ${f}"
done

get_fs_test_dir_path
for f in TestRedfishFileSystem.java
do
    ln -s "${REDFISH_SRC_BASE}/hadoop/test/${f}" \
        "${HADOOP_PATH}/${FS_TEST_DIR_PATH}/${f}" \
            || die "failed to link ${f}"
done

# NOTE: you may want to add libhfishc to the native Hadoop libraries folder,
# using a command such as this:
# ln -s /home/cmccabe/tmp/redfish/hadoop/libhfishc.so
#     /media/fish/hadoop-0.20.203.0/lib/native/Linux-amd64-64/libhfishc.so 
