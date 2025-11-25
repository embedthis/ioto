#!/usr/bin/env bash
#
#   setup.sh - TestMe setup script to start web
#

set -m

# Find web.json5 in current directory or parent directory
if [ -f "web.json5" ]; then
    CONFIG="web.json5"
elif [ -f "../web.json5" ]; then
    CONFIG="../web.json5"
else
    echo "Error: Cannot find web.json5" >&2
    exit 1
fi

ENDPOINT=`json 'web.listen[0]' $CONFIG`
if [ -z "$ENDPOINT" ]; then
    echo "Error: Cannot get endpoint from $CONFIG" >&2
    exit 1
fi

if curl -s ${ENDPOINT}/ >/dev/null 2>&1; then
    echo "Web is already running on ${ENDPOINT}"
    sleep 999999 &
else
    echo "Starting web with config: $CONFIG"
    web --config $CONFIG --trace web.log:all:all &
fi
PID=$!

cleanup() {
    kill $PID
    exit 0
}

trap cleanup SIGINT SIGTERM SIGQUIT EXIT

wait $PID

