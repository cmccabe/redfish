#!/usr/bin/python

from __future__ import with_statement
import struct

RF_PAYLOAD_HDR_LEN = 16

DEFAULT_MDS_MDS_PORT = 9000

MMMD_GET_MDS_STATUS = 3000
MMMD_MDS_STATUS = 3001

# Highest transaction ID that has yet been used by this script.
highest_trid = 1

def rf_msg_serialize(ty, payload):
    global highest_trid

    trid = highest_trid
    highest_trid = highest_trid + 1
    rem_trid = 0
    length = len(payload) + RF_PAYLOAD_HDR_LEN
    ty = ty
    pad = 0
    return (struct.pack(">IIIHH", rem_trid, trid, length, ty, pad), trid)

def rf_msg_deserialize(ty, payload):
    if (ty == MMMD_GET_MDS_STATUS):
        return RfMsgGetMdsStatus.deserialize(payload)
    elif (ty == MMMD_MDS_STATUS):
        return RfMsgMdsStatus.deserialize(payload)
    else:
        raise RuntimeError("can't understand message type %d" % ty)

class RfMsgGetMdsStatus(object):
    def __init__(self):
        pass
    def serialize(self):
        return rf_msg_serialize(MMMD_GET_MDS_STATUS, "")
    @staticmethod
    def deserialize(payload):
        return RfMsgGetMdsStatus()

class RfMsgMdsStatus(object):
    def __init__(self, mid):
        self.mid = mid
    def serialize(self, mid):
        payload = struct.pack(">H", mid)
        return rf_msg_serialize(MMMD_GET_MDS_STATUS, payload)
    @staticmethod
    def deserialize(payload):
        mid = struct.unpack(">H", payload)[0]
        return RfMsgMdsStatus(mid)

class RfFailedToSendMsg(Exception):
    def __init__(self, value):
        self.value = value
    def __str__(self):
        return repr(self.value)

class RfFailedToRecvReply(Exception):
    def __init__(self, value):
        self.value = value
    def __str__(self):
        return repr(self.value)
