#!/usr/bin/env bash
#
#   setup.sh - Start web server for benchmark testing
#

set -m

ENDPOINT=$(json 'web.listen[0]' web.json5)

if [ -z "$ENDPOINT" ]; then
    echo "Error: Cannot get endpoint from web.json5" >&2
    exit 1
fi

if curl -s ${ENDPOINT}/ >/dev/null 2>&1; then
    echo "Web is already running on ${ENDPOINT}"
    sleep 999999 &
else
    echo "Starting web server for benchmarks on ${ENDPOINT}"
    web --trace web.log &
fi
PID=$!

# Save PID for monitoring
echo $PID > bench.pid

cleanup() {
    kill $PID 2>/dev/null
    rm -f bench.pid
    exit 0
}

trap cleanup SIGINT SIGTERM SIGQUIT EXIT

wait $PID
