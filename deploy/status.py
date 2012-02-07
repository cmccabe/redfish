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
(opts, args, jo) = parse_deploy_opts(parser)
jo = load_conf_file(opts.cluster_conf)

diter = DaemonIter.from_conf_object(jo, None)
i = 0
for d in diter:
    i = i + 1
    print "processing daemon %d" % i
    print d.jo
    try:
        pid = d.run_with_output("cat " + d.get_pid_file())
        print "daemon " + str(i) + " is running as process " + pid
    except CalledProcessError, e:
        print "daemon " + str(i) + " is NOT running."
