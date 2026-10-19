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


#
#   Probe for a server already serving this suite. Test for a successful response and for the
#   document the healthcheck uses: curl exits 0 on a 404 or any other error response, so its
#   plain exit status cannot tell "no server" from "a server that is not ours".
#
probe() {
    curl -sf -o /dev/null --max-time 2 ${ENDPOINT}/index.html 2>/dev/null
}

if probe; then
    echo "Web is already running on ${ENDPOINT}"
    sleep 999999 &
    PID=$!
else
    echo "Starting web server for leak test on ${ENDPOINT}"
    web --config $CONFIG --trace web.log &
    PID=$!
    #
    #   Wait for the server to answer before returning, so tests cannot race the startup.
    #   Report the port as the cause if it never comes up -- an orphaned server from an
    #   interrupted run holds the port and makes the bind fail silently.
    #
    for i in $(seq 1 50); do
        if probe; then
            break
        fi
        if ! kill -0 $PID 2>/dev/null; then
            echo "Error: web exited immediately. Is ${ENDPOINT} already in use?" >&2
            lsof -nP -iTCP:${ENDPOINT##*:} -sTCP:LISTEN >&2 2>/dev/null
            exit 1
        fi
        sleep 0.1
    done
    if ! probe; then
        echo "Error: web did not answer on ${ENDPOINT}/index.html within 5 seconds" >&2
        kill $PID 2>/dev/null
        exit 1
    fi
fi

# Save PID for leak test to monitor
echo $PID > web.pid

cleanup() {
    kill $PID 2>/dev/null
    rm -f web.pid
    exit 0
}

trap cleanup SIGINT SIGTERM SIGQUIT EXIT

wait $PID
