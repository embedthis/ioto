#!/bin/bash
#
#   setup.sh - TestMe setup script for fuzzing
#

set -m

mkdir -p corpus crashes crashes-archive

web &
PID=$!

cleanup() {
    kill $PID
    exit 0
}

trap cleanup SIGINT SIGTERM SIGQUIT EXIT

wait $PID


