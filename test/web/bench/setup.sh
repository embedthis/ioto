#!/usr/bin/env bash
#
#   setup.sh - Start web server for benchmark testing
#

set -m

ENDPOINT=$(json 'web.listen[0]' web.json5)
WEB_STARTED=0  # Track if we started the web server

if [ -z "$ENDPOINT" ]; then
    echo "Error: Cannot get endpoint from web.json5" >&2
    exit 1
fi

if curl -s ${ENDPOINT}/ >/dev/null 2>&1; then
    # Find the existing web server PID
    if [[ "$OSTYPE" == "darwin"* ]] || [[ "$OSTYPE" == "linux"* ]]; then
        PID=$(lsof -ti :4260 2>/dev/null | head -1)
    else
        # Windows Git Bash
        PID=$(netstat -ano 2>/dev/null | grep ':4260.*LISTEN' | awk '{print $NF}' | head -1)
    fi
    if [ -z "$PID" ]; then
        echo "Error: Cannot find web server process" >&2
        exit 1
    fi
    # Save web server PID for monitoring
    echo $PID > bench.pid
    echo "Web is already running on ${ENDPOINT} with PID $PID"
    sleep 999999 &
    WAITING_PID=$!
else
    echo "Starting web server for benchmarks on ${ENDPOINT}"
    web --trace web.log &
    WAITING_PID=$!
    # Save web server PID for monitoring
    echo $WAITING_PID > bench.pid
fi

cleanup() {
    kill $WAITING_PID 2>/dev/null
    rm -f bench.pid
    exit 0
}

trap cleanup SIGINT SIGTERM EXIT

wait $WAITING_PID
