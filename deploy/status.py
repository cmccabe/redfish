#!/usr/bin/python

from optparse import OptionParser
from subprocess import *
import json
import of_daemon
import of_node
import of_util
import os
import sys
import tempfile

of_util.check_python_version()
parser = OptionParser()
(opts, args, node_list) = of_util.parse_deploy_opts(parser)

diter = of_node.OfNodeIter(node_list, ["daemon"])
for d in diter:
    try:
        pid = d.run_with_output("cat " + d.get_pid_path(), {})
        print d.get_short_name() + " is running as process " + pid
    except CalledProcessError, e:
        print d.get_short_name() + " is NOT running."
