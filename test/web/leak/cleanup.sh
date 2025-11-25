#!/usr/bin/env bash
#
#   cleanup.sh - Cleanup after leak test
#

# Only clean up logs if test succeeded
if [ "${TESTME_SUCCESS}" = "1" ]; then
    rm -f web.log
    rm -f leak-results.txt
else
    echo "Test failed - preserving logs:"
    [ -f web.log ] && echo "  web.log"
    [ -f leak-results.txt ] && echo "  leak-results.txt"
fi

# Always clean up temporary files
rm -f web.pid
rm -f tmp/*
rm -f .leak-memory-samples.txt
