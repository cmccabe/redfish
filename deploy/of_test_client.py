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

""" Represents a Redfish test client.
FIXME: this code doesn't yet handle goofy filenames correctly
FIXME: should distinguish between command failures and ssh failures
"""
class OfTestClient(of_node.OfNode):
    @staticmethod
    def from_json(id, jd):
        return OfTestClient(id, jd["base_dir"], jd["host"], {})
    def __init__(self, id, base_dir, host, labels):
        labels["test_client"] = 1
        of_node.OfNode.__init__(self, id, base_dir, host, labels)
        self.bin_path = base_dir + "/usr/stest"
        self.active_tests = []
    def get_binary_name(self):
        return self.id.get_binary_name()
    def get_short_name(self):
        return "cli%d" % self.id
    # test: the test itself.  example: "st_mkdirs"
    # tsid: the test ID, which uniquely identifies a test run.  example: "run1"
    #       This defaults to the test name
    def start_stest(self, params):
        if (not params.has_key("tsid")):
            params["tsid"] = params["test"]
        test_base_dir = "%s/%s" % (self.base_dir, params["tsid"])
        bin_path = "%s/%s" % (self.bin_path, params["test"])
        self.run("rm -rf '%s'" % test_base_dir, {})
        self.run("mkdir -p '%s'" % test_base_dir, {})
        self.run("'%s' -d '%s'" % (bin_path, test_base_dir), {})
        pid = self.run_with_output("cat '%s/pid'" % test_base_dir, {})
        pid = int(pid)
        test = OfTest(params["test"], params["tsid"], test_base_dir,
                    bin_path, pid)
        self.active_tests.append(test)
    def join_stests(self):
        results = {}
        while (len(self.active_tests)):
            test = self.active_tests[0]
            if (test.is_running(self)):
                sleep(1)
                continue
            result = test.get_result(self)
            results[test.tsid] = result
            self.active_tests = self.active_tests[1:]
        return results
    def join_stests_expect_success(self):
        saw_errors = False
        results = self.join_stests()
        for tsid in results.keys():
            if (results[tsid].err_txt != ""):
                print "*** TEST ERROR FROM %s" % tsid
                print results[tsid].err_txt
                saw_errors = True
        if (saw_errors):
            raise RuntimeError("join_stests_expect_success saw errors.")

class OfTest(object):
    def __init__(self, test, tsid, test_base_dir, bin_path, pid):
        self.test = test
        self.tsid = tsid
        self.test_base_dir = test_base_dir
        self.bin_path = bin_path
        self.pid = pid
    def is_running(self, test_client):
        try:
            # Does the process still exist?
            test_client.run("ps %d &>/dev/null" % self.pid, {})
            return True
        except subprocess.CalledProcessError, e:
            return False
    def kill(self, test_client):
        if (not self.is_running(test_client)):
            return
        test_client.run("kill %d" % self.pid, {})
    def get_result(self, test_client):
        err_txt = test_client.run_with_output("cat '%s/err'" % \
            (self.test_base_dir), {})
        return OfTestResult(err_txt)

class OfTestResult(object):
    def __init__(self, err_txt):
        self.err_txt = err_txt
