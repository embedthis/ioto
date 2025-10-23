#!/usr/bin/env bash
#
#   setup.sh - TestMe setup script to start ioto
#

set -m

ENDPOINT=`json 'listen[0]' ./state/config/web.json5`

if curl -s "${ENDPOINT}" >/dev/null 2>&1; then
    echo "Ioto is already running"
    sleep 999999 &
    PID=$!
else
    ioto --trace log.txt:all:all &
    PID=$!
fi

cleanup() {
    if [ "${TESTME_SUCCESS}" = "0" ] ; then
        cat log.txt >&2
    fi
    kill $PID >/dev/null 2>&1
    exit 0
}

trap cleanup SIGINT SIGTERM SIGQUIT EXIT

wait $PID

