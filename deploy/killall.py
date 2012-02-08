#!/usr/bin/python

import json
import os
import subprocess
import sys
import tempfile
from of_daemon import *
from of_util import *
from optparse import OptionParser

check_python_version()
parser = OptionParser()
parser.add_option("-9", "--kill-9", action="store_true", dest="kill9")
(opts, args, jo) = parse_deploy_opts(parser)
diter = DaemonIter.from_conf_object(jo, None)

for d in diter:
    print "processing " + d.get_short_name() 
    try:
        pid = d.run_with_output("cat " + d.get_pid_file()).rstrip()
    except subprocess.CalledProcessError, e:
        continue
    if (opts.kill9 == True):
        try:
            d.run_with_output("rm -f " + d.get_pid_file())
            d.run("kill -9 " + str(pid))
            d.run("rm -f " + d.get_pid_file())
        except subprocess.CalledProcessError, e:
            pass
    else:
        d.run("kill " + str(pid))
        d.run("rm -f " + d.get_pid_file())
