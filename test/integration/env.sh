#!/bin/bash
#
#   env.sh - Set the test environment and echo to stdout.
#

ENDPOINT=`json 'listen[0]' ./state/config/web.json5`/api/test
export ENDPOINT

echo "ENDPOINT=${ENDPOINT}"
exit 0
