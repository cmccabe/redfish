#!/bin/bash

find -executable -type f | grep _unit\$  | xargs -l valgrind --leak-check=full
