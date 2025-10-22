#!/bin/bash
#
#   setup.sh - TestMe setup script to start web
#

set -m

ENDPOINT=`json 'listen[0]' ./state/config/web.json5`

if url -q ${ENDPOINT}/ >/dev/null 2>&1; then
    echo "Web is already running on port 4100"
    sleep 999999 &
    PID=$!
else
    echo "Starting web"
    web &
    PID=$!
fi

cleanup() {
    kill $PID
    exit 0
}

trap cleanup SIGINT SIGTERM SIGQUIT EXIT

wait $PID

