#!/usr/bin/python

from optparse import OptionParser
import json
import of_daemon
import of_node
import of_util
import os
import subprocess
import sys

of_util.check_python_version()
parser = OptionParser()
parser.add_option("-9", "--kill-9", action="store_true", dest="kill9")
parser.add_option("--unlink", action="store_true", dest="unlink")
(opts, args, node_list) = of_util.parse_deploy_opts(parser)

# Kill all daemons
for n in of_node.OfNodeIter(node_list, []):
    print "processing " + n.get_short_name() 
    n.kill(opts.kill9 == True)
    if (opts.unlink == True):
        n.run("rm -rf '" + n.base_dir + "'", {})

# Kill all open sockets for ssh connection sharing
of_node.kill_ssh_connection_sharing_daemons(node_list)
