#!/bin/bash -ex

D="`dirname $0`"
"$D/killall.py"
"$D/install.py"
"$D/run.py"
