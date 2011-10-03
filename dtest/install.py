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

parser = OptionParser()
parser.add_option("-c", "--cluster-config", dest="cluster_conf")
parser.add_option("-b", "--redfish-build-directory", dest="bld_dir")
(opts, args) = parser.parse_args()
if opts.cluster_conf == None:
    sys.stderr.write("you must give a RedFish cluster configuration file\n")
    sys.exit(1)
if opts.bld_dir == None:
    sys.stderr.write("you must give a RedFish build directory\n")
    sys.exit(1)

jo = load_conf_file(opts.cluster_conf)

#os.path.join(sys.path[0], sys.argv[0])
bdir = os.path.join(tempfile.gettempdir(), str(os.getpid()))
bdir = bdir + "/"
os.mkdir(bdir)
os.chdir(opts.bld_dir)
subprocess.check_call(["make", "install", ("DESTDIR=" + bdir)])

diter = DaemonIter.from_conf_object(jo, None)
i = 0
for d in diter:
    i = i + 1
    print "processing daemon %d" % i
    print d.jo
    # Create base_dir where logs and configuration will go 
    d.run("mkdir -p " + d.get_base_dir())
    # Upload daemon configuration file
    with tempfile.NamedTemporaryFile(delete=False) as f:
        f.write(json.dumps(d.jo, sort_keys=True, indent=4))
        f.flush()
        d.upload(f.name, d.get_conf_file())
    # upload system binaries
    d.upload(bdir, d.get_base_dir())
