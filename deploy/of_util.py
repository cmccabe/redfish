#!/usr/bin/python

#from of_daemon import *
from optparse import OptionParser
from subprocess import *
import json
import of_daemon
import of_node
import os
import subprocess
import sys
import tempfile

#
# Some common routines for the deployment scripts
#

def check_python_version():
    if sys.version < '2.5':
        sys.stderr.write("You need Python 2.5 or newer.)\n")
        sys.exit(1)

def parse_deploy_opts(parser):
    parser.add_option("-c", "--cluster-config", dest="cluster_conf")
    parser.add_option("-b", "--build-directory", dest="bld_dir")
    (opts, args) = parser.parse_args()
    if opts.cluster_conf == None:
        opts.cluster_conf = os.getenv("REDFISH_CONF")
        if opts.cluster_conf == None:
            sys.stderr.write("you must give a Redfish cluster configuration file\n")
            sys.exit(1)
    if opts.bld_dir == None:
        opts.bld_dir = os.getenv("REDFISH_BUILD_DIR")
    jo = of_daemon.load_conf_file(opts.cluster_conf)
    node_list = of_node.node_list_from_conf_object(jo)
    opts.conf_json = jo
    return (opts, args, node_list)

# FIXME: implement me!
def shell_quote(str):
    return str

def subprocess_check_output(*popenargs, **kwargs):
    if 'stdout' in kwargs:
        raise ValueError('stdout argument not allowed, it will be overriden')
    process = Popen(stdout=PIPE, *popenargs, **kwargs)
    output, unused_err = process.communicate()
    retcode = process.poll()
    if retcode:
        cmd = kwargs.get("args")
        if cmd is None:
            cmd = popenargs[0]
        raise CalledProcessError(retcode, cmd)
    return output

def safe_recv(sock, rem):
    ret = ""
    while True: 
        res = sock.recv(rem)
        if (res == ""):
            raise RuntimeError("unexpected EOF on recv")
        ret += res
        rem -= len(ret)
        if (rem == 0):
            break
    return ret
