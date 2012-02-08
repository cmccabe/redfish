#!/usr/bin/python

import json
import os
import subprocess
import sys
import tempfile
from of_daemon import *
from of_util import *
from optparse import OptionParser

def process_is_running(pid):
    try:
        subprocess_check_output(["ps", "-p", str(pid)])
        return True
    except:
        return False

check_python_version()
parser = OptionParser()
(opts, args, jo) = parse_deploy_opts(parser)
jo = load_conf_file(opts.cluster_conf)

diter = DaemonIter.from_conf_object(jo, None)
for d in diter:
    print "processing " + d.get_short_name()
    print d.jo
    try:
        pid = d.run_with_output("cat " + d.get_pid_file())
        if (process_is_running(int(pid))):
            print "error: daemon " + d.get_short_name() + " is still running as process " + pid
            continue
        else:
            os.unlink(d.get_pid_file())
    except subprocess.CalledProcessError, e:
        pass
    d.run(d.get_binary_path() +  " -c " +  d.get_conf_file() +
        " -k " + str(d.id.idx))
