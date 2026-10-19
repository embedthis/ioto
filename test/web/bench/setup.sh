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


#
#   Probe for a server already serving this suite. Test for a successful response and for the
#   document the healthcheck uses: curl exits 0 on a 404 or any other error response, so its
#   plain exit status cannot tell "no server" from "a server that is not ours".
#
probe() {
    curl -sf -o /dev/null --max-time 2 ${ENDPOINT}/index.html 2>/dev/null
}

if probe; then
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
    web --test-actions --trace web.log &
    WAITING_PID=$!
    # Save web server PID for monitoring
    echo $WAITING_PID > bench.pid
    #
    #   Wait for the server to answer before returning, so tests cannot race the startup.
    #   Report the port as the cause if it never comes up -- an orphaned server from an
    #   interrupted run holds the port and makes the bind fail silently.
    #
    for i in $(seq 1 50); do
        if probe; then
            break
        fi
        if ! kill -0 $WAITING_PID 2>/dev/null; then
            echo "Error: web exited immediately. Is ${ENDPOINT} already in use?" >&2
            lsof -nP -iTCP:${ENDPOINT##*:} -sTCP:LISTEN >&2 2>/dev/null
            exit 1
        fi
        sleep 0.1
    done
    if ! probe; then
        echo "Error: web did not answer on ${ENDPOINT}/index.html within 5 seconds" >&2
        kill $WAITING_PID 2>/dev/null
        exit 1
    fi
fi

cleanup() {
    kill $WAITING_PID 2>/dev/null
    rm -f bench.pid
    exit 0
}

trap cleanup SIGINT SIGTERM EXIT

wait $WAITING_PID
