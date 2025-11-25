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

# Get server endpoint from web.json5
ENDPOINT=`json 'web.listen[0]' web.json5`
if [ -z "$ENDPOINT" ]; then
    echo "Error: Cannot get endpoint from web.json5" >&2
    exit 1
fi
rm -f .testme/existing

# Check if server is already running
if curl -s ${ENDPOINT}/ >/dev/null 2>&1; then
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
    # Sleep forever to keep service running
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
        if curl -s ${ENDPOINT}/ >/dev/null 2>&1; then
            # echo "Server is ready"
            break
        fi
        sleep 0.5
    done
    PID=$SERVER_PID
fi

# Background monitor for server crashes
(
    while kill -0 $SERVER_PID 2>/dev/null; do
        sleep 0.5
    done
    # Server died
    EXIT_CODE=$?
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" >&2
    echo "⚠️  SERVER CRASHED at $(date)" >&2
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" >&2
    echo "Server PID: $SERVER_PID" >&2
    echo "Exit code: $EXIT_CODE" >&2

    # Check for core dump
    if ls core.* core 2>/dev/null; then
        echo "Core dump found - saving to crashes/server/" >&2
        mkdir -p crashes/server
        mv core.* core crashes/server/ 2>/dev/null || true
    fi
    echo "$(date): Server exited (PID $SERVER_PID, exit code $EXIT_CODE)" >> crashes/server-crash.log
) &
MONITOR_PID=$!

# Cleanup function
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

# Wait for server process
wait $PID
