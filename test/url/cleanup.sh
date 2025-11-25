#!/bin/bash
#
#   TestMe cleanup script
#

if [ "${TESTME_SUCCESS}" = "1" ]; then
    rm -f web.log
else
    cat web.log
fi
rm -f tmp/*.tmp
