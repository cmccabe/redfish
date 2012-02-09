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
    d.kill(opts.kill9 == True)
