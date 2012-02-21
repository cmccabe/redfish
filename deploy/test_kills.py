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
(opts, args, node_list) = of_util.parse_deploy_opts(parser)

for d in of_node.OfNodeIter(node_list, ["mds"]):
    print "checking status of " + d.get_short_name()
    ret = d.check_mds_status()
    print d.get_short_name() + " has status " + str(ret)

for d in of_node.OfNodeIter(node_list, ["mds"]):
    d.kill(False) 

for d in of_node.OfNodeIter(node_list, ["mds"]):
    print "verifying that " + d.get_short_name() + " is dead..."
    try:
        ret = d.check_mds_status()
        raise RuntimeError(d.get_short_name() + " is still alive!")
    except Exception as e:
        pass

print "All MDSes have been killed."
