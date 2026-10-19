#!/usr/bin/env bash
#
#   setup.sh - TestMe setup script for fuzzing
#

echo "Setting up fuzz test environment..."

# Create necessary directories
mkdir -p corpus crashes crashes/server crashes-archive .testme

# Enable core dumps for crash analysis
ulimit -c unlimited
echo "Core dumps enabled: $(ulimit -c)"

STATUS_FILE=.testme/server.status
rm -f .testme/existing .testme/server.pid "$STATUS_FILE"

# Register cleanup before startup so early failures also release resources.
cleanup() {
    echo "Cleaning up server processes..."
    # Kill monitor if running
    if [ -n "$MONITOR_PID" ]; then
        kill $MONITOR_PID 2>/dev/null || true
    fi
    # Kill server/sleep
    if [ -n "$PID" ]; then
        kill $PID 2>/dev/null || true
        wait $PID 2>/dev/null || true
    fi
    exit 0
}

trap cleanup SIGINT SIGTERM SIGQUIT EXIT

# Get server endpoint from web.json5
ENDPOINT=`json 'web.listen[0]' web.json5`
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

# Check if server is already running
if probe; then
    echo "Web server already running on ${ENDPOINT}"

    # Try to find existing web process
    EXISTING_PID=$(pgrep -n web)
    if [ -n "$EXISTING_PID" ]; then
        echo "Using existing server PID: $EXISTING_PID"
        SERVER_PID=$EXISTING_PID
        echo $EXISTING_PID > .testme/server.pid
        touch .testme/existing
    else
        echo "Cannot find existing server pid"
        exit 1
    fi
# A reused server is not our child; only liveness can be checked.
    sleep 999999 &
    PID=$!
else
    echo "Starting web server on ${ENDPOINT}"

    if [ "$REBUILD" = "1" -a "$SANITIZE" = "1" ]; then
        echo "Building server with sanitizers..."
        make -C ../.. clean
        CFLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1" \
        LDFLAGS="-fsanitize=address,undefined" \
        make -C ../..
    fi

    # Start web server in background
    web --trace 'web.log:error,!debug:all,!mbedtls' &
    SERVER_PID=$!

    # Store PID for crash detection
    echo $SERVER_PID > .testme/server.pid
    echo "Web server started with PID: $SERVER_PID"

    # Wait for server to be ready
    echo "Waiting for server to be ready..."
    for i in {1..30}; do
        if probe; then
            # echo "Server is ready"
            break
        fi
        sleep 0.5
    done
    PID=$SERVER_PID
fi

# Only the parent can collect exit status. Allow time for its status file to appear.
(
    while kill -0 $SERVER_PID 2>/dev/null; do
        sleep 0.5
    done
    for i in 1 2 3 4 5; do
        [ -f "$STATUS_FILE" ] && break
        sleep 0.2
    done
    STATUS=$(cat "$STATUS_FILE" 2>/dev/null || echo "unknown (not our child)")
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" >&2
    echo "⚠️  SERVER CRASHED at $(date)" >&2
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" >&2
    echo "Server PID: $SERVER_PID" >&2
    echo "Status: $STATUS" >&2

    # Check for core dump
    if ls core.* core 2>/dev/null; then
        echo "Core dump found - saving to crashes/server/" >&2
        mkdir -p crashes/server
        mv core.* core crashes/server/ 2>/dev/null || true
    fi
    echo "$(date): Server exited (PID $SERVER_PID, status $STATUS)" >> crashes/server-crash.log
) &
MONITOR_PID=$!

# As the server's parent, collect its exit status here.
wait $PID 2>/dev/null
CODE=$?
if [ "$PID" = "$SERVER_PID" ] && [ ! -f "$STATUS_FILE" ]; then
    if [ $CODE -gt 128 ]; then
        echo "signal:$((CODE - 128))" > "$STATUS_FILE"
    else
        echo "exit:$CODE" > "$STATUS_FILE"
    fi
fi
