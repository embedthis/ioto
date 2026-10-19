#!/usr/bin/env bash
#
#   setup.sh - TestMe setup script to start web
#

set -m

. ./server-id.sh

if probe; then
    echo "Web is already running on ${ENDPOINT}"
    #
    #   We did not start this server, so cleanup must not kill it -- a developer running one by
    #   hand is exactly what this branch is for. Watch it instead. If it goes away mid-run every
    #   remaining test fails against nothing, which reads as dozens of unrelated assertion
    #   failures rather than a missing server.
    #
    (
        while probe; do
            sleep 1
        done
        echo "" >&2
        echo "Error: the web server on ${ENDPOINT} went away mid-run." >&2
        echo "       Test failures after this point are a missing server, not the tests." >&2
    ) &
    PID=$!
elif occupied; then
    refuseForeign
    exit 1
else
    echo "Starting web on ${ENDPOINT} with config: $CONFIG"
    #
    #   --test-actions registers the /test action table the suite exercises. It is unauthenticated
    #   by construction, so the server no longer registers it just because the build is a debug
    #   build - the request has to be explicit, and this is the only place that makes it.
    #
    web --config $CONFIG --test-actions --trace web.log:all:all > web.out 2>&1 &
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
