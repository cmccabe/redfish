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
parser.add_option("-b", "--redfish-build-directory", dest="bld_dir")
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

for d in of_node.OfNodeIter(node_list, ["daemon"]):
    print "installing " + d.get_short_name()
    # Create base_dir where logs and configuration will go 
    d.run("mkdir -p " + d.base_dir)
    # Upload daemon configuration file
    with tempfile.NamedTemporaryFile(delete=False) as f:
        f.write(json.dumps(opts.conf_json, sort_keys=True, indent=4))
        f.flush()
        d.upload(f.name, d.conf_path)
    # upload system binaries
    d.upload(install_dir, d.base_dir)

for c in of_node.OfNodeIter(node_list, ["test_client"]):
    print "installing " + c.get_short_name()
    d.run("mkdir -p " + d.base_dir)
