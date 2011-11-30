#!/usr/bin/python

import json
import os
import subprocess
import sys
import tempfile
from of_daemon import *
from of_util import *
from optparse import OptionParser

if sys.version < '2.5':
    sys.stderr.write("You need Python 2.5 or newer.)\n")
    sys.exit(1)

def process_is_running(pid):
    try:
        subprocess_check_output(["ps", "-p", str(pid)])
        return True
    except:
        return False

parser = OptionParser()
parser.add_option("-c", "--cluster-config", dest="cluster_conf")
(opts, args) = parser.parse_args()
if opts.cluster_conf == None:
    sys.stderr.write("you must give a RedFish cluster configuration file\n")
    sys.exit(1)

jo = load_conf_file(opts.cluster_conf)

diter = DaemonIter.from_conf_object(jo, None)
i = 0
for d in diter:
    i = i + 1
    print "processing daemon %d" % i
    print d.jo
    try:
        pid = d.run_with_output("cat " + d.get_pid_file())
        if (process_is_running(int(pid))):
            print "error: daemon " + str(i) + " is still running as process " + pid
            continue
        else:
            os.unlink(d.get_pid_file())
    except subprocess.CalledProcessError, e:
        pass
    d.run(d.get_binary_path() +  " -c " +  d.get_conf_file() +
        " -k " + str(d.id.idx))
