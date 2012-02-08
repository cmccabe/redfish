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

diter = DaemonIter.from_conf_object(jo, None)
for d in diter:
    if d.id.ty != DaemonId.MDS:
        continue
    print "checking status of " + d.get_short_name()
    ret = d.check_mds_status()
    print d.get_short_name() + " has status " + str(ret)
