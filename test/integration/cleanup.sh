#!/usr/bin/env bash
#
#   TestMe cleanup script
#

if [ "${TESTME_SUCCESS}" = "1" ] ; then
    echo rm -f ./log.txt
fi
