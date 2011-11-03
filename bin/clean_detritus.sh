#!/bin/sh

find -name '*~' -print0 | xargs -0 -l -r rm -f
