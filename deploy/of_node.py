#!/usr/bin/python

from __future__ import with_statement
from socket import *
import json
import of_daemon
import of_msg
import of_util
import os
import signal
import string
import subprocess
import sys
import tempfile

of_util.check_python_version()

def get_ssh_control_path_setup_cmd(host):
    ssh_cmd = get_ssh_cmd(host)
    ssh_cmd.append("-fMN")
    return ssh_cmd

""" Get the path to the ssh control socket to be used for a given host.
This is so that you can use SSH connection sharing to speed up your test rig. """
def get_ssh_control_path(host):
    return os.path.join(os.path.expanduser(os.environ['SSH_CONTROL_PATH']),
                ("master-%s" % host))

""" Get the SSH command to ssh to a given host """
def get_ssh_cmd(host):
    cmd = [ "ssh", "-o", "PasswordAuthentication=no", "-x" ]
    if os.environ.has_key('SSH_CONTROL_PATH'):
        cmd.append("-S")
        cmd.append(get_ssh_control_path(host))
    return cmd

def create_ssh_connection_sharing_daemons(node_list):
    if not os.environ.has_key('SSH_CONTROL_PATH'):
        return
    all_hosts = {}
    for n in OfNodeIter(node_list, []):
        all_hosts[n.host] = 1
    for host in all_hosts.keys():
        cn_path = get_ssh_control_path(host)
        if (not os.path.exists(cn_path)):
            ssh_cmd = get_ssh_control_path_setup_cmd(host)
            ssh_cmd.append(host)
            print "creating sshd " + ' '.join(ssh_cmd)
            subprocess.check_call(ssh_cmd)

def kill_ssh_connection_sharing_daemons(node_list):
    if not os.environ.has_key('SSH_CONTROL_PATH'):
        return
    all_hosts = {}
    for n in OfNodeIter(node_list, []):
        all_hosts[n.host] = 1
    ssh_cmds_to_kill = []
    for host in all_hosts.keys():
        ssh_cmd = get_ssh_control_path_setup_cmd(host)
        ssh_cmds_to_kill.append(' '.join(ssh_cmd))
    p = subprocess.Popen(['ps', 'aux'], stdout=subprocess.PIPE)
    out, err = p.communicate()
    for line in out.splitlines():
        for s in ssh_cmds_to_kill:
            if s in line:
                pid = int(line.split()[1])
                print "killing sshd " + str(pid)
                os.kill(pid, signal.SIGKILL)
    for host in all_hosts.keys():
        p = get_ssh_control_path(host)
        if (os.path.exists(p)):
            print "unlinking " + p
            os.unlink(p)

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
        ssh_cmd = get_ssh_cmd(self.host)
        ssh_cmd.append(self.host)
        ssh_cmd.append(cmd)
        return of_util.subprocess_check_output(ssh_cmd)
    """ Run a command on this node. Throws an exception on failure. """
    def run(self, cmd):
        ssh_cmd = get_ssh_cmd(self.host)
        ssh_cmd.append(self.host)
        ssh_cmd.append(cmd)
        subprocess.check_call(ssh_cmd)
    """ Upload a file to this node. Throws an exception on failure. """
    def upload(self, local_path, remote_path):
        ssh_cmd = get_ssh_cmd(self.host)
        ssh_cmd_str = ' '.join(ssh_cmd)
        subprocess.check_call([ "rsync", "-r", "-e",
                ssh_cmd_str, local_path, (self.host + ":" + remote_path)])
    """ Download a file from this node. Throws an exception on failure. """
    def download(self, local_path, remote_path):
        ssh_cmd = get_ssh_cmd(self.host)
        ssh_cmd_str = ' '.join(ssh_cmd)
        subprocess.check_call([ "rsync", "-r", "-e",
                ssh_cmd_str, (self.host + ":" + remote_path), local_path])
    """ Determine if this node has a certain label """
    def has_label(self, label):
        return self.labels.has_key(label)
    """ Determine if this node has all of a list of labels """
    def has_all_labels(self, want_labels):
        for want in want_labels:
            if (not self.has_label(want)):
                return False
        return True
    def upload_conf(self, conf_json):
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(json.dumps(conf_json, sort_keys=True, indent=4))
            f.flush()
            self.upload(f.name, self.conf_path)

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

import of_test_client

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
    if (jo.has_key("test_client")):
        id = -1
        for jd in jo["test_client"]:
            id = id + 1
            node_list.append(of_test_client.OfTestClient.from_json(id, jd))
    return node_list

