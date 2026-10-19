#!/usr/bin/env bash
#
#   receive-timeout.tst.sh - A peer that dribbles a request must not hold its fiber for minutes
#
#   The server serves each connection on a fiber and holds it for the whole request, so the fiber
#   pool - not web.limits.connections - is the real ceiling on connections serviced at once. Two
#   refresh rules made that ceiling cheap to occupy:
#
#     1. webConsumeBody called webUpdateDeadline for every byte taken from the buffer, resetting the
#        deadline to the inactivity window and, byte by byte, out to the full request timeout. One
#        byte at a time therefore held a fiber for minutes while sending almost nothing.
#     2. serveRequest applied the tight parse budget only to the FIRST request on a connection. A
#        keep-alive connection got the inactivity timeout instead, so a partial request line on a
#        connection already used once was cheaper to hold than a new connection.
#
#   Both are now bounded from the start of the request and are not refreshed: web.timeouts.receive
#   caps a buffered body read, and the parse budget applies to every request.
#
#   This measures elapsed time, because that is what the defect is about - the status code is the
#   same either way. Each budget is set to a couple of seconds and the request timeout to a minute,
#   so the two outcomes are far apart and the assertion is not a race: on the unfixed server each
#   case runs to the request or inactivity timeout and the wait here expires.
#
set -u

PORT=4975
CONFIG=tmp/receive-timeout.json5
LOG=tmp/receive-timeout.log
RECEIVE=2
PARSE=2
LIMIT=10          # generous next to a 2s budget, far below the 60s the unfixed server takes

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
            body: '100K',
            connections: '100',
        },
        timeouts: {
            parse: '${PARSE} secs',
            receive: '${RECEIVE} secs',
            inactivity: '60 secs',
            request: '60 secs',
        },
        routes: [ { match: '', handler: 'file' } ],
    },
}
EOF

web --config "$CONFIG" --quiet > "$LOG" 2>&1 &
PID=$!
trap 'kill -9 $PID 2>/dev/null' EXIT

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

status=0

#
#   Run one dribbling client and report how long the server kept the connection. nc exits when the
#   server closes, so the elapsed time is the hold.
#
hold() {
    local start end pid i
    start=$(date +%s)
    #   No trailing sleep: nc keeps the TCP connection open after its stdin ends and exits only
    #   when the SERVER closes, which is exactly the interval being measured. Holding the pipe open
    #   instead measures the client's own sleep and the test then passes or fails for that reason.
    printf "$1" | nc 127.0.0.1 $PORT > /dev/null 2>&1 &
    pid=$!
    #   Bounded here rather than by "nc -w", which does not cap an established connection on every
    #   platform. Without a cap the unfixed server runs to its request timeout and the harness kills
    #   the test, which reports a timeout instead of the assertion that actually failed.
    for i in $(seq 1 $((LIMIT * 10))) ; do
        kill -0 $pid 2>/dev/null || break
        sleep 0.1
    done
    kill -9 $pid 2>/dev/null
    wait $pid 2>/dev/null
    end=$(date +%s)
    echo $((end - start))
}

#
#   A complete header block declaring a large body, then one byte and silence. webReadBody loops
#   until the declared length arrives, and every byte used to buy another inactivity window.
#
elapsed=$(hold 'POST /index.html HTTP/1.1\r\nHost: x\r\nContent-Length: 100000\r\n\r\nA')
if [ "$elapsed" -lt $((RECEIVE + 6)) ] ; then
    echo "✓ a dribbled body was cut off after ${elapsed}s, inside the ${RECEIVE}s receive budget"
else
    echo "✗ a dribbled body held the connection for ${elapsed}s; the receive budget is ${RECEIVE}s"
    echo "      an incomplete body is refreshing its own deadline"
    status=1
fi

#
#   A complete request, then the request line of a second one and silence. The second request is
#   where the parse budget used to be dropped, so this is the keep-alive half.
#
elapsed=$(hold 'GET /index.html HTTP/1.1\r\nHost: x\r\n\r\nGET /index.html HTTP/1.1\r\n')
if [ "$elapsed" -lt $((PARSE + 6)) ] ; then
    echo "✓ a partial keep-alive request was cut off after ${elapsed}s, inside the ${PARSE}s parse budget"
else
    echo "✗ a partial keep-alive request held the connection for ${elapsed}s; the parse budget is ${PARSE}s"
    echo "      the parse budget is not being applied after the first request"
    status=1
fi

#
#   The control: the server is unharmed and still serving. Without this a server that simply died
#   would pass both assertions above, since a dead server closes connections promptly.
#
code=$(curl --noproxy '*' -s -o /dev/null -m 5 -w '%{http_code}' "http://127.0.0.1:${PORT}/index.html" 2>/dev/null)
if [ "$code" = "200" ] ; then
    echo "✓ the server is still serving normally"
else
    echo "✗ the server answered '${code}' after the dribbling clients"
    status=1
fi

exit $status
