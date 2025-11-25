#!/bin/bash
#
#   setup.sh - TestMe setup script to start web
#

set -m

ENDPOINT=`json web.listen[0] web.json5`
if [ -z "$ENDPOINT" ]; then
    echo "Error: Cannot get endpoint from web.json5" >&2
    exit 1
fi

if curl -s ${ENDPOINT}/ >/dev/null 2>&1; then
    echo "Web is already running on ${ENDPOINT}"
    sleep 999999 &
else
    echo "Starting web"
    web --trace web.log:all:all &
fi
PID=$!

cleanup() {
    kill $PID 2>/dev/null
    exit 0
}

trap cleanup SIGINT SIGTERM SIGQUIT EXIT

wait $PID
