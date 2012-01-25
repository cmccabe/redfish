#!/usr/bin/python

from __future__ import with_statement
from of_util import *
import json
import string
import subprocess
import sys

if sys.version < '2.5':
    sys.stderr.write("You need Python 2.5 or newer.)\n")
    sys.exit(1)

def load_conf_file(conf_file):
    cstr = ""
    # FIXME: be clever enough to allow users to put hash marks inside JSON strings
    with open(conf_file, mode='r') as f:
        while True:
            line = f.readline()
            if not line:
                break
            hash_idx = string.find(line, "#")
            if (hash_idx == -1):
                cstr = cstr + line
            else:
                cstr = cstr + line[:hash_idx] +  "\n"
    print cstr
    jo = json.loads(cstr)
    validate_conf_file(jo)
    return jo

def validate_conf_file(jo):
    if not jo.has_key("mds"):
        raise RuntimeError("validate_conf_file: failed to find /mds\n")
    if not jo.has_key("osd"):
        raise RuntimeError("validate_conf_file: failed to find " +
            "/osd\n")
    i = 0
    for d in jo["mds"]:
        i = i + 1
        if not d.has_key("base_dir"):
            raise RuntimeError("validate_conf_file: mds %d does not " +
                "have a base_dir parameter" % i)
    i = 0
    for d in jo["osd"]:
        i = i + 1
        if not d.has_key("base_dir"):
            raise RuntimeError("validate_conf_file: osd %d does not " +
                "have a base_dir parameter" % i)

def has_label(arr, label):
    if (label == None):
        return True
    for a in arr:
        if a == label:
            return True
    return False

class DaemonId(object):
    MDS = 1
    OSD = 2
    def __init__(self, ty, idx):
        self.ty = ty
        self.idx = idx
    def __repl__(self):
        if (self.ty == DaemonId.MDS):
            return "MDS%03d" % self.idx
        elif (self.ty == DaemonId.OSD):
            return "OSD%03d" % self.idx
        else:
            raise Exception("unknown type " + type)
    def get_binary_name(self):
        if (self.ty == DaemonId.MDS):
            return "fishmds"
        elif (self.ty == DaemonId.OSD):
            return "fishosd"
        else:
            raise Exception("unknown type " + self.ty)

class DaemonIter(object):
    @staticmethod
    def from_conf_object(jo, label):
        return DaemonIter(jo, label)
    def __init__(self, jo, label):
        self.jo = jo
        self.label = label
        self.ty = DaemonId.MDS
        self.idx = -1
    def __iter__(self):
        return self
    def next(self):
        while (True):
            if (self.ty == DaemonId.MDS):
                darr = self.jo["mds"]
            elif (self.ty == DaemonId.OSD):
                darr = self.jo["osd"]
            else:
                raise RuntimeError("Illegal DaemonIter state")
            self.idx = self.idx + 1
            if (self.idx >= len(darr)):
                if (self.ty == DaemonId.MDS):
                    self.ty = DaemonId.OSD
                    self.idx = -1
                    continue
                else:
                    raise StopIteration
            if has_label(darr[self.idx], self.label):
                return Daemon(self.jo, darr[self.idx],
                        DaemonId(self.ty, self.idx + 1))

""" Represents a Redfish daemon.
FIXME: this code doesn't yet handle goofy filenames correctly
FIXME: should distinguish between command failures and ssh failures
"""
class Daemon(object):
    def __init__(self, jo, jd, id):
        self.jo = jo
        self.jd = jd
        self.id = id
    """ Run a command on this daemon and give output. Throws an exception on failure. """
    def run_with_output(self, cmd):
        return subprocess_check_output([ "ssh", "-o",
            "PasswordAuthentication=no", "-x", self.jd["host"], cmd])
    """ Run a command on this daemon. Throws an exception on failure. """
    def run(self, cmd):
        subprocess.check_call([ "ssh", "-o",
            "PasswordAuthentication=no", "-x", self.jd["host"], cmd])
    """ Upload a file to this daemon. Throws an exception on failure. """
    def upload(self, local_path, remote_path):
        subprocess.check_call([ "rsync", "-r", "-e",
                    "ssh -o PasswordAuthentication=no -x",
                    local_path,
                    (self.jd["host"] + ":" + remote_path)])
    """ Download a file from this daemon. Throws an exception on failure. """
    def download(self, local_path, remote_path):
        subprocess.check_call([ "rsync", "-r", "-e",
                    "ssh -o PasswordAuthentication=no -x",
                    (self.jd["host"] + ":" + remote_path),
                    local_path])
    """ Generate a Redfish config file for this daemon """ 
    def generate_daemon_conf(self, cmd):
        return self.jo
    def get_base_dir(self):
        return self.jd["base_dir"]
    def get_conf_file(self):
        return self.jd["base_dir"] + "/conf"
    def get_pid_file(self):
        return self.jd["base_dir"] + "/pid"
    def get_binary_name(self):
        return self.id.get_binary_name()
    def get_binary_path(self):
        return self.jd["base_dir"] + "/usr/bin/" + self.get_binary_name()
