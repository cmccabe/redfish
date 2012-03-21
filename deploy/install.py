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
if opts.bld_dir == None:
    sys.stderr.write("you must give a Redfish build directory\n")
    sys.exit(1)

# If we've configured ssh connection sharing, create the ssh control sockets
# here
of_node.kill_ssh_connection_sharing_daemons(node_list)
of_node.create_ssh_connection_sharing_daemons(node_list)

# Build the binaries to install
install_dir = os.path.join(tempfile.gettempdir(), str(os.getpid()))
install_dir = install_dir + "/"
os.mkdir(install_dir)
os.chdir(opts.bld_dir)
subprocess.check_call(["make", "install", ("DESTDIR=" + install_dir)])

for n in of_node.OfNodeIter(node_list, []):
    print "installing " + n.get_short_name()
    # Create base_dir where logs and configuration will go 
    n.run("mkdir -p " + n.base_dir, {})
    # Upload configuration file
    n.upload_conf(opts.conf_json)
    # upload system binaries
    n.upload(install_dir, n.base_dir)
