#!/bin/bash
#
#   setup.sh - TestMe setup script to start web
#

set -m

#   Must match the healthcheck in testme.json5
PROBE=index.html

ENDPOINT=`json web.listen[0] web.json5`
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
    curl -sf -o /dev/null --max-time 2 ${ENDPOINT}/${PROBE} 2>/dev/null
}

if probe; then
    echo "Web is already running on ${ENDPOINT}"
    sleep 999999 &
    PID=$!
else
    echo "Starting web on ${ENDPOINT}"
    #
    #   --test-actions registers the /test action table these tests exercise. It is unauthenticated
    #   by construction, so the server no longer registers it just because the build is a debug
    #   build - the request has to be explicit, and this is the only place that makes it.
    #
    web --test-actions --trace web.log:all:all > web.out 2>&1 &
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
            wait $PID
            status=$?
            #
            #   A silent immediate exit is almost always the loader, not the port: on Windows a
            #   missing DLL kills the process before main() runs and prints nothing, which is
            #   indistinguishable from a bind failure unless the exit status is reported.
            #
            echo "Error: web exited immediately with status ${status}" >&2
            echo "  web resolves to: $(command -v web 2>/dev/null || echo 'NOT FOUND ON PATH')" >&2
            echo "  PATH=${PATH}" >&2
            if [ -s web.out ]; then echo "  web output:" >&2; sed 's/^/    /' web.out >&2; fi
            if [ -s web.log ]; then echo "  web trace:" >&2; tail -20 web.log | sed 's/^/    /' >&2; fi
            if [ -s upload-acl.log ]; then echo "  upload directory:" >&2; sed 's/^/    /' upload-acl.log >&2; fi
            lsof -nP -iTCP:${ENDPOINT##*:} -sTCP:LISTEN >&2 2>/dev/null
            exit 1
        fi
        sleep 0.1
    done
    if ! probe; then
        echo "Error: web did not answer on ${ENDPOINT}/${PROBE} within 5 seconds" >&2
        kill $PID 2>/dev/null
        exit 1
    fi
fi

cleanup() {
    kill $PID 2>/dev/null
    exit 0
}

trap cleanup SIGINT SIGTERM SIGQUIT EXIT

wait $PID
