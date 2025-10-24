#!/usr/bin/env bash
#
#   cleanup.sh - TestMe cleanup script
#

# set -m

if [ "${TESTME_KEEP}" = "" ] ; then
    rm -f log.txt
fi

