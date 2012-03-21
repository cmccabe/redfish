#!/usr/bin/python

from optparse import OptionParser
import filecmp
import json
import of_daemon
import of_node
import of_util
import os
import subprocess
import sys
import tempfile
import time

of_util.check_python_version()
parser = OptionParser()
(opts, args, node_list) = of_util.parse_deploy_opts(parser)
if opts.bld_dir == None:
    sys.stderr.write("you must give a Redfish build directory\n")
    sys.exit(1)

# get a chunk ID that we think will be unique
cid = int(time.clock())
cid  = cid + (os.getpid() << 32)

# create input file
input_file = opts.bld_dir + "/hello.in"
f = open(input_file, "w")
try:
    print >>f, "hello, world!"
finally:
    f.close()

output_file = opts.bld_dir + "/hello.out"

for d in of_node.OfNodeIter(node_list, ["osd"]):
    print "writing chunk to " + d.get_short_name()
    tool_cmd = [ opts.bld_dir + "/tool/fishtool", "chunk_write", 
        "-i", input_file, "-k", str(d.id), hex(cid) ]
    of_util.subprocess_check_output(tool_cmd)

for d in of_node.OfNodeIter(node_list, ["osd"]):
    print "reading chunk from " + d.get_short_name()
    tool_cmd = [ opts.bld_dir + "/tool/fishtool", "chunk_read", 
        "-o", output_file, "-k", str(d.id), hex(cid) ]
    of_util.subprocess_check_output(tool_cmd)
    filecmp.cmp(input_file, output_file)
