#!/usr/bin/python

from optparse import OptionParser
import json
import of_daemon
import of_node
import of_util
import os
import subprocess
import sys
import tempfile

of_util.check_python_version()
parser = OptionParser()
(opts, args, node_list) = of_util.parse_deploy_opts(parser)

stests = []

for d in of_node.OfNodeIter(node_list, ["test_client"]):
    print "starting tests on " + d.get_short_name() 
    stests.append(d.start_stest({ "test" : "st_startup"}))
    stests.append(d.start_stest({ "test" : "st_trivial"}))
    stests.append(d.start_stest({ "test" : "st_write_then_read"}))
    stests.append(d.start_stest({ "test" : "st_mkdirs"}))
    stests.append(d.start_stest({ "test" : "st_unlink"}))
    stests.append(d.start_stest({ "test" : "st_rename1"}))

for d in of_node.OfNodeIter(node_list, ["test_client"]):
    d.join_stests_expect_success(stests)
print "tests completed successfully."
