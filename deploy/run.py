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

def process_is_running(pid):
    try:
        subprocess_check_output(["ps", "-p", str(pid)])
        return True
    except:
        return False

parser = OptionParser()
(opts, args, node_list) = of_util.parse_deploy_opts(parser)
for d in of_node.OfNodeIter(node_list, ["daemon"]):
    try:
        pid = d.run_with_output("cat " + d.get_pid_path(), {})
        if (process_is_running(int(pid))):
            print "error: daemon " + d.get_short_name() + " is still running as process " + pid
            continue
        else:
            os.unlink(d.get_pid_path())
    except CalledProcessError, e:
        pass
    d.run(d.bin_path +  " -c " +  d.conf_path + " -k " + str(d.id), {})
