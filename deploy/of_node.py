#!/usr/bin/python

from __future__ import with_statement
from socket import *
import json
import of_daemon
import of_msg
import of_util
import string
import subprocess
import sys

of_util.check_python_version()

""" Represents a Redfish node.
FIXME: this code doesn't yet handle goofy filenames correctly
FIXME: should distinguish between command failures and ssh failures
"""
class OfNode(object):
    def __init__(self, id, base_dir, host, labels):
        self.id = id
        self.base_dir = base_dir
        self.host = host
        self.labels = labels
        self.conf_path = base_dir + "/conf"
    """ Run a command on this node and give output. Throws an exception on failure. """
    def run_with_output(self, cmd):
        return of_util.subprocess_check_output([ "ssh", "-o",
            "PasswordAuthentication=no", "-x", self.host, cmd])
    """ Run a command on this node. Throws an exception on failure. """
    def run(self, cmd):
        subprocess.check_call([ "ssh", "-o",
            "PasswordAuthentication=no", "-x", self.host, cmd])
    """ Upload a file to this node. Throws an exception on failure. """
    def upload(self, local_path, remote_path):
        subprocess.check_call([ "rsync", "-r", "-e",
                    "ssh -o PasswordAuthentication=no -x",
                    local_path,
                    (self.host + ":" + remote_path)])
    """ Download a file from this node. Throws an exception on failure. """
    def download(self, local_path, remote_path):
        subprocess.check_call([ "rsync", "-r", "-e",
                    "ssh -o PasswordAuthentication=no -x",
                    (self.host + ":" + remote_path),
                    local_path])
    """ Determine if this node has a certain label """
    def has_label(self, label):
        return self.labels.has_key(label)
    """ Determine if this node has all of a list of labels """
    def has_all_labels(self, want_labels):
        for want in want_labels:
            if (not self.has_label(want)):
                return False
        return True

""" Create a list of OfNode objects from a JSON configuration object
representing the unitary configuration file. """
def node_list_from_conf_object(jo):
    node_list = []
    if (not jo.has_key("mds")):
        raise RuntimeError("malformed configuration file: no MDS list.")
    id = -1
    for m in jo["mds"]:
        id = id + 1
        node_list.append(of_daemon.OfMds.from_json(id, m))
    if (not jo.has_key("osd")):
        raise RuntimeError("malformed configuration file: no OSD list.")
    id = -1
    for m in jo["osd"]:
        id = id + 1
        node_list.append(of_daemon.OfOsd.from_json(id, m))
    if (jo.has_key("test_clients")):
        for m in jo["test_clients"]:
            node_list.append(OfTestClient.from_json(id, m))
    return node_list

class OfNodeIter(object):
    def __init__(self, node_list, want_labels):
        self.node_list = node_list
        self.want_labels = want_labels
        self.idx = -1
    def __iter__(self):
        return self
    def next(self):
        while (True):
            self.idx = self.idx + 1
            if (self.idx >= len(self.node_list)):
                raise StopIteration
            node = self.node_list[self.idx]
            if (not node.has_all_labels(self.want_labels)):
                continue
            return node
