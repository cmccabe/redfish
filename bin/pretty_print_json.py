#!/usr/bin/python

from __future__ import with_statement
import json
import os
import sys

contents = sys.stdin.read()
jo = json.loads(contents)
print json.dumps(jo, sort_keys=True, indent=2)
