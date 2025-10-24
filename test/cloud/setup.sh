#!/usr/bin/env bash
#
#   setup.sh - TestMe setup script to start ioto
#

# set -m

ENDPOINT=`json 'listen[0]' ./state/config/web.json5`

if url -q ${ENDPOINT}/ >/dev/null 2>&1; then
    echo "Ioto is already running on port 9030"
    sleep 999999 &
    PID=$!
else
    echo "Starting ioto"
    rm -f log.txt
    ioto --trace ./log.txt &
    PID=$!
fi

cleanup() {
    if [ "${TESTME_VERBOSE}" = "1" ] ; then
        cat log.txt >&2
    fi
    kill $PID >/dev/null 2>&1
    exit 0
}

trap cleanup SIGINT SIGTERM SIGQUIT EXIT

wait $PID

