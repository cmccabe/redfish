#!/bin/sh

#
# strip_trailing_whitespace.sh
#
# Strips the trailing whitespace in .c, .h, and .jorm files.
#

die() {
    echo $@
    exit 1
}

clean_file() {
    sed --in-place 's/[ \t]*$//' "$1"
}

DNAME="`dirname \"$0\"`"
DNAME="`readlink -f \"$DNAME\"`"
PROJ_ROOT="$DNAME/.."

find -name '*.c' -type f -print0 | while read -d $'\0' file; do
    clean_file "$file"
done

find -name '*.h' -type f -print0 | while read -d $'\0' file; do
    clean_file "$file"
done

find -name '*.jorm' -type f -print0 | while read -d $'\0' file; do
    clean_file "$file"
done

exit 0
