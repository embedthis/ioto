#!/usr/bin/env bash
#   Hold partial requests below the connection limit and verify normal requests still run. Explicit fiber limits
#   below connection capacity must warn.
set -u

PORT=4976
CONFIG=tmp/fiber-ceiling.json5
LOG=tmp/fiber-ceiling.log
LOW_CONFIG=tmp/fiber-ceiling-low.json5
LOW_LOG=tmp/fiber-ceiling-low.log
HOLDERS=12

command -v web >/dev/null 2>&1 || { echo "SKIP: the web command is not on PATH" ; exit 77 ; }
command -v nc  >/dev/null 2>&1 || { echo "SKIP: nc is not available" ; exit 77 ; }
mkdir -p tmp

cat > "$CONFIG" <<EOF
{
    web: {
        documents: 'site',
        index: 'index.html',
        listen: [ 'http://127.0.0.1:${PORT}' ],
        limits: {
            connections: '100',
        },
        timeouts: {
            parse: '60 secs',
            inactivity: '60 secs',
            request: '60 secs',
        },
        routes: [ { match: '', handler: 'file' } ],
    },
}
EOF

web --config "$CONFIG" --quiet > "$LOG" 2>&1 &
PID=$!
HELD=()
trap 'kill -9 $PID ${HELD[@]+"${HELD[@]}"} 2>/dev/null' EXIT

for _ in $(seq 1 50) ; do
    if [ "$(curl --noproxy '*' -s -o /dev/null -m 5 -w '%{http_code}' \
            "http://127.0.0.1:${PORT}/index.html" 2>/dev/null)" = "200" ] ; then
        break
    fi
    sleep 0.1
done
if ! kill -0 $PID 2>/dev/null ; then
    echo "✗ the server did not stay up"
    cat "$LOG"
    exit 1
fi

#   Partial-request holders outlive the test via the parse timeout. Detach output so they cannot hold the
#   harness pipe open.
for _ in $(seq 1 $HOLDERS) ; do
    { printf 'GET /index.html HTTP/1.1\r\n' ; sleep 8 ; } 2>/dev/null | nc 127.0.0.1 $PORT > /dev/null 2>&1 &
    HELD+=($!)
done
sleep 1

status=0
code=$(curl --noproxy '*' -s -o /dev/null -m 5 -w '%{http_code}' "http://127.0.0.1:${PORT}/index.html" 2>/dev/null)
if [ "$code" = "200" ] ; then
    echo "✓ a request was served while ${HOLDERS} half-sent requests held their connections"
else
    echo "✗ a request got '${code}' while ${HOLDERS} half-sent requests held their connections"
    echo "      the fiber ceiling is below web.limits.connections"
    status=1
fi

kill -9 $PID ${HELD[@]+"${HELD[@]}"} 2>/dev/null
wait $PID 2>/dev/null

cat > "$LOW_CONFIG" <<EOF
{
    limits: {
        fibers: 10,
    },
    web: {
        documents: 'site',
        listen: [ 'http://127.0.0.1:${PORT}' ],
        limits: {
            connections: '100',
        },
        routes: [ { match: '', handler: 'file' } ],
    },
}
EOF
web --config "$LOW_CONFIG" > "$LOW_LOG" 2>&1 &
PID=$!
for _ in $(seq 1 50) ; do
    grep -q 'limits.fibers is 10 but web.limits.connections is 100' "$LOW_LOG" && break
    sleep 0.1
done
if grep -q 'limits.fibers is 10 but web.limits.connections is 100' "$LOW_LOG" ; then
    echo "✓ an explicit limits.fibers below web.limits.connections is reported"
else
    echo "✗ an explicit limits.fibers of 10 with 100 connections was not reported"
    cat "$LOW_LOG"
    status=1
fi

exit $status
