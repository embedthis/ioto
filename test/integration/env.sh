#!/bin/bash
#
#   env.sh - Set the test environment and echo to stdout.
#

ENDPOINT=`json 'listen[0]' ./state/config/web.json5`/api/test
export ENDPOINT

echo @@ STDERR ENV.SH ENDPOINT=${ENDPOINT} >&2

echo "ENDPOINT=${ENDPOINT}"
exit 0
