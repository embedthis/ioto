#!/usr/bin/env bash
#
#   setup.sh - Start web server for leak testing
#

set -m

CONFIG="web.json5"
ENDPOINT=$(json 'web.listen[0]' $CONFIG)

if [ -z "$ENDPOINT" ]; then
    echo "Error: Cannot get endpoint from $CONFIG" >&2
    exit 1
fi

if curl -s ${ENDPOINT}/ >/dev/null 2>&1; then
    echo "Web is already running on ${ENDPOINT}"
    sleep 999999 &
else
    echo "Starting web server for leak test on ${ENDPOINT}"
    web --config $CONFIG --trace web.log &
fi
PID=$!

# Save PID for leak test to monitor
echo $PID > web.pid

cleanup() {
    kill $PID 2>/dev/null
    rm -f web.pid
    exit 0
}

trap cleanup SIGINT SIGTERM SIGQUIT EXIT

wait $PID
