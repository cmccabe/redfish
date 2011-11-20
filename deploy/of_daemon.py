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
    if not jo.has_key("cluster"):
        raise RuntimeError("validate_conf_file: failed to find /cluster\n")
    if not jo["cluster"].has_key("daemons"):
        raise RuntimeError("validate_conf_file: failed to find " +
            "/daemons/cluster\n")
    i = 0
    for d in jo["cluster"]["daemons"]:
        i = i + 1
        if not d.has_key("base_dir"):
            raise RuntimeError("validate_conf_file: daemon %d does not " +
                "have a base_dir parameter" % i)

def has_label(arr, label):
    if (label == None):
        return True
    for a in arr:
        if a == label:
            return True
    return False

class DaemonIter(object):
    @staticmethod
    def from_conf_object(jo, label):
        return DaemonIter(jo["cluster"]["daemons"], label)
    def __init__(self, darr, label):
        self.darr = darr
        self.label = label
        self.idx = -1
    def __iter__(self):
        return self
    def next(self):
        while (True):
            self.idx = self.idx + 1
            if (self.idx >= len(self.darr)):
                raise StopIteration
            if has_label(self.darr[self.idx], self.label):
                return Daemon(self.darr[self.idx])

""" Represents a RedFish daemon.
FIXME: this code doesn't yet handle goofy filenames correctly
FIXME: should distinguish between command failures and ssh failures
"""
class Daemon(object):
    def __init__(self, jo):
        self.jo = jo
    """ Run a command on this daemon and give output. Throws an exception on failure. """
    def run_with_output(self, cmd):
        return subprocess_check_output([ "ssh", "-o",
            "PasswordAuthentication=no", "-x", self.jo["host"], cmd])
    """ Run a command on this daemon. Throws an exception on failure. """
    def run(self, cmd):
        subprocess.check_call([ "ssh", "-o",
            "PasswordAuthentication=no", "-x", self.jo["host"], cmd])
    """ Upload a file to this daemon. Throws an exception on failure. """
    def upload(self, local_path, remote_path):
        subprocess.check_call([ "rsync", "-r", "-e",
                    "ssh -o PasswordAuthentication=no -x",
                    local_path,
                    (self.jo["host"] + ":" + remote_path)])
    """ Download a file from this daemon. Throws an exception on failure. """
    def download(self, local_path, remote_path):
        subprocess.check_call([ "rsync", "-r", "-e",
                    "ssh -o PasswordAuthentication=no -x",
                    (self.jo["host"] + ":" + remote_path),
                    local_path])
    """ Generate a RedFish config file for this daemon """ 
    def generate_daemon_conf(self, cmd):
        jd = {}
        CKEYS = [
            # base daemon stuff
            "port",
            "rack",
            # log_config
            "use_syslog",
            "base_dir",
            "crash_log",
            "fast_log",
            "glitch_log",
            "pid_file",
        ]
        for k in CKEYS:
            if self.jo.has_key(k):
                jd[i] =self.jo.get(k)
        return jd
    def get_base_dir(self):
        return self.jo["base_dir"]
    def get_conf_file(self):
        return self.jo["base_dir"] + "/conf"
    def get_pid_file(self):
        return self.jo["base_dir"] + "/pid"
    def get_binary_name(self):
        if (self.jo["type"] == "mds"):
            return "fishmds"
        elif (self.jo["type"] == "osd"):
            return "fishosd"
        else:
            raise Exception("unknown type " + type)
    def get_binary_path(self):
        return self.jo["base_dir"] + "/usr/bin/" + self.get_binary_name()
