#!/bin/bash

die() {
    echo $@
    exit 1
}

FISHMON="fishmon"
[ -x ./mon/fishmon ] && FISHMON="./mon/fishmon"
FISHTOP="fishtop"
[ -x ./top/fishtop ] && FISHTOP="./top/fishtop"
[ -x ${FISHMON} ] || die "can't locate fishmon"
[ -x ${FISHTOP} ] || die "can't locate fishtop"

CONF=""
while getopts  ":c:h" flag
do
    case $flag in
    c) CONF=$OPTARG;;
    h) ${FISHMON} -h; exit 0;;
    *) ;;
    esac
done

[ -z "${CONF}" ] && die "you must specify the monitor configuration with -c"
[ -f "${CONF}" ] || die "The monitor configuration file was not found."

$FISHMON $@ || die
$FISHTOP -c "${CONF}" || die
