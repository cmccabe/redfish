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
parser.add_option("-b", "--redfish-build-directory", dest="bld_dir")
(opts, args, jo) = parse_deploy_opts(parser)
if opts.bld_dir == None:
    sys.stderr.write("you must give a Redfish build directory\n")
    sys.exit(1)
install_dir = os.path.join(tempfile.gettempdir(), str(os.getpid()))
install_dir = install_dir + "/"
os.mkdir(install_dir)
os.chdir(opts.bld_dir)
subprocess.check_call(["make", "install", ("DESTDIR=" + install_dir)])

diter = DaemonIter.from_conf_object(jo, None)
for d in diter:
    print "processing " + d.get_short_name()
    # Create base_dir where logs and configuration will go 
    d.run("mkdir -p " + d.get_base_dir())
    # Upload daemon configuration file
    with tempfile.NamedTemporaryFile(delete=False) as f:
        f.write(json.dumps(d.jo, sort_keys=True, indent=4))
        f.flush()
        d.upload(f.name, d.get_conf_file())
    # upload system binaries
    d.upload(install_dir, d.get_base_dir())
