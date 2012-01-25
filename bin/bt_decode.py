#!/usr/bin/python

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
# bt_decode.py
#
# This script decodes the numeric addresses given by the GNU backtrace(3)
# function into file names and line numbers.
# 
# EXAMPLE USAGE:
# INPUT:
# ./mds/mstor_unit[0x41bf1e]
# /lib64/libpthread.so.0[0x395e20f4a0]
# ./mds/mstor_unit[0x412513]
# ./mds/mstor_unit(main+0x17d)[0x412ca0]
# /lib64/libc.so.6(__libc_start_main+0xfd)[0x395da1ecdd]
# ./mds/mstor_unit[0x4104a9]
# 
# OUTPUT:
# ./mds/mstor_unit[0x41bf1e]              /media/fish/redfish/core/signal.c:80
# /lib64/libpthread.so.0[0x395e20f4a0]    sigaction.c:0
# ./mds/mstor_unit[0x412513]              /media/fish/redfish/mds/mstor_unit.c:543
# ./mds/mstor_unit(main+0x17d)[0x412ca0]  /media/fish/redfish/mds/mstor_unit.c:615
# /lib64/libc.so.6(__libc_start_main+0xfd)[0x395da1ecdd]  ??:0
# ./mds/mstor_unit[0x4104a9]              ??:0
#

import os
import re
import string
import sys

pat = re.compile('([^\[]*)\[([^\]]*)\]')

for line in sys.stdin:
    line = string.rstrip(line)
    mat = pat.match(line)
    if (not mat):
        continue
    exe = mat.group(1)
    e = exe.find("(")
    if (e != -1):
        exe = exe[0:e]
    addr = mat.group(2)
    print "%s\t" % (string.ljust(line, 50)),
    sys.stdout.flush()
    os.system("addr2line -e %s %s" % (exe, addr))
