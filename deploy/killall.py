#!/usr/bin/python

from optparse import OptionParser
import json
import of_daemon
import of_node
import of_util
import os
import subprocess
import sys
import tempfile

of_util.check_python_version()
parser = OptionParser()
parser.add_option("-9", "--kill-9", action="store_true", dest="kill9")
(opts, args, node_list) = of_util.parse_deploy_opts(parser)

for d in of_node.OfNodeIter(node_list, ["daemon"]):
    print "processing " + d.get_short_name() 
    d.kill(opts.kill9 == True)
