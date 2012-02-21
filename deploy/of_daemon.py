#!/usr/bin/python

from __future__ import with_statement
from socket import *
import json
import of_msg
import of_node
import of_util
import string
import struct
import subprocess
import sys

of_util.check_python_version()

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
    return jo

class OfDaemon(of_node.OfNode):
    def __init__(self, id, base_dir, host, labels):
        labels["daemon"] = 1
        of_node.OfNode.__init__(self, id, base_dir, host, labels)
        self.bin_path = base_dir + "/usr/bin/" + self.get_binary_name()
    def get_pid_path(self):
        return self.base_dir + "/pid"
    def get_conf_path(self):
        return self.base_dir + "/conf"
    def get_binary_name(self):
        return self.id.get_binary_name()
    def kill(self, kill9):
        try:
            pid = self.run_with_output("cat " + self.get_pid_path()).rstrip()
        except subprocess.CalledProcessError, e:
            return False
        if (kill9 == True):
            try:
                self.run_with_output("rm -f " + self.get_pid_path())
                self.run("kill -9 " + str(pid))
                self.run("rm -f " + self.get_pid_path())
            except subprocess.CalledProcessError, e:
                return
        else:
            self.run("kill " + str(pid))
            self.run("rm -f " + self.get_pid_path())
    def rf_msgr_send(self, msg):
        try:
            sock = socket(AF_INET, SOCK_STREAM)
            sock.connect((self.host, self.debug_port))
            (bytes, trid) = msg.serialize()
            sock.sendall(bytes)
        except Exception as e:
            raise of_msg.RfFailedToSendMsg(e.args)
        sock.close()
    def rf_msgr_send_with_resp(self, msg):
        try:
            sock = socket(AF_INET, SOCK_STREAM)
            sock.connect((self.host, self.debug_port))
            (bytes, trid) = msg.serialize()
            sock.sendall(bytes)
        except Exception as e:
            raise of_msg.RfFailedToSendMsg(e.args)
        hdr = None
        try:
            hdr = of_util.safe_recv(sock, of_msg.RF_PAYLOAD_HDR_LEN)
        except Exception as e:
            raise of_msg.RfFailedToRecvReply(e.args)
        (utrid, rem_trid, length, ty, pad) = struct.unpack(">IIIHH", hdr)
        if (length < of_msg.RF_PAYLOAD_HDR_LEN):
            raise of_msg.RfFailedToRecvReply("received a message with length %d, \
which is too short" % length)
        if (utrid != trid):
            raise of_msg.RfFailedToRecvReply("received a reply message with \
trid %d, but we sent out trid %d" % (utrid, trid))
        rem_length = length - of_msg.RF_PAYLOAD_HDR_LEN
        payload = ""
        if (rem_length > 0):
            try:
                payload = of_util.safe_recv(sock, rem_length)
            except Exception as e:
                raise of_msg.RfFailedToRecvReply(e.args)
        sock.close()
        return of_msg.rf_msg_deserialize(ty, payload)

class OfMds(OfDaemon):
    @staticmethod
    def from_json(id, jd):
        labels = {}
        if (jd.has_key("labels")):
            labels = jd["labels"]
        if (jd.has_key("mds_port")):
            debug_port = jd["mds_port"]
        else:
            self.debug_port = of_msg.DEFAULT_MDS_MDS_PORT
        return OfMds(id, jd["base_dir"], jd["host"], debug_port, labels)
    def __init__(self, id, base_dir, host, debug_port, labels):
        labels["mds"] = 1
        self.debug_port = debug_port
        OfDaemon.__init__(self, id, base_dir, host, labels) 
    def get_binary_name(self):
        return "fishmds"
    def get_short_name(self):
        return "mds%d" % self.id
    def check_mds_status(self):
        msg = self.rf_msgr_send_with_resp(of_msg.RfMsgGetMdsStatus())
        if (not isinstance(msg, of_msg.RfMsgMdsStatus)):
            raise RuntimeError("got wrong message type response from \
RfMsgGetMdsStatus.")
        if (self.id != msg.mid):
            raise RuntimeError("MDS status said its mid is " + str(msg.mid) + " but we \
think it's " + str(self.id))
        return (msg.mid)

class OfOsd(OfDaemon):
    @staticmethod
    def from_json(id, jd):
        labels = {}
        if (jd.has_key("labels")):
            labels = jd["labels"]
        return OfOsd(id, jd["base_dir"], jd["host"], labels)
    def __init__(self, id, base_dir, host, labels):
        labels["osd"] = 1
        OfDaemon.__init__(self, id, base_dir, host, labels) 
    def get_binary_name(self):
        return "fishosd"
    def get_short_name(self):
        return "osd%d" % self.id
