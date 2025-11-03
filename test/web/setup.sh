#!/bin/bash
#
#   setup.sh - TestMe setup script to start web
#

set -m

ENDPOINT=`json 'web.listen[0]' web.json5`
if [ -z "$ENDPOINT" ] ; then
    echo "Cannot get listen endpoint" >&2
    exit 1
fi

if url -q ${ENDPOINT}/ >/dev/null 2>&1; then
    echo "Web is already running on ${ENDPOINT}"
    sleep 999999 &
    PID=$!
else
    echo "Starting web"
    web --trace log.txt:all:all &
    PID=$!
fi

cleanup() {
    if [ "${TESTME_VERBOSE}" = "1" ] ; then
        cat log.txt >&2
    fi
    kill $PID 2>/dev/null
    exit 0
}

trap cleanup SIGINT SIGTERM SIGQUIT EXIT

wait $PID

