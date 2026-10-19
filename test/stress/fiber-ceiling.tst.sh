#!/usr/bin/env bash
#
#   fiber-ceiling.tst.sh - Slow connections well inside web.limits.connections must not deny service
#
#   The fiber pool, not web.limits.connections, is the real ceiling on concurrently serviced
#   connections. The web server spawns one fiber per accepted socket and holds it for the whole
#   request - including every wait on a slow peer - and rAllocFiber refuses once the pool is full, at
#   which point acceptSocket closes the next connection with no response at all. limits.fibers
#   defaulted to 4 while every shipped template advertised connections: 100, so four half-sent
#   requests denied service to the entire process: the web server and, because the pool is
#   process-wide, every background subsystem with it.
#
#   The default is now derived from web.limits.connections with headroom for the agent's own
#   background fibers, so the advertised number is the one that binds.
#
#   HOLDERS is deliberately above the old default of 4 and far below the configured 100. On the
#   unfixed agent the ordinary request below is closed without a response; on the fixed one it is
#   answered while the holders are still mid-request.
#
#   What this does NOT test: how long one connection may hold a fiber. A slow peer can still occupy
#   its fiber for the full request timeout, so enough of them still exhaust the larger pool. Bounding
#   a peer-paced hold is a separate change in the web read path.
#
set -u

ENDPOINT="${TM_HTTP:-http://localhost:4480}"
#   127.0.0.1, not the endpoint's "localhost": the listener binds IPv4 loopback, and localhost
#   resolves to ::1 first here, so a raw nc connection to the name never establishes.
HOSTNAME_=$(echo "$ENDPOINT" | sed -e 's|http://||' -e 's|:.*||')
HOST=127.0.0.1
PORT=$(echo "$ENDPOINT" | sed -e 's|.*:||')
HOLDERS=12
status=0

#   --noproxy: the endpoint is loopback and a proxy in the environment must not be consulted for it.
curl_() {
    curl --noproxy '*' "$@"
}

alive() {
    [ "$(curl_ -s -o /dev/null -m 5 -w '%{http_code}' "${ENDPOINT}/index.html" 2>/dev/null)" = "200" ]
}

command -v nc >/dev/null 2>&1 || { echo "SKIP: nc is not available" ; exit 77 ; }

for _ in $(seq 1 100) ; do
    alive && break
    sleep 0.1
done
if ! alive ; then
    echo "✗ the agent is not answering on ${ENDPOINT} before the test starts"
    exit 1
fi

#
#   Each holder sends a complete header block declaring a large body, then one body byte, then
#   stops. webReadBody loops until the declared length arrives, so the fiber is held - but the
#   connection is live and well formed, so nothing rejects it.
#
PIDS=""
for i in $(seq 1 $HOLDERS) ; do
    (
        printf 'POST /api/public/upload HTTP/1.1\r\nHost: %s\r\nContent-Length: 1000000\r\n\r\nA' "$HOSTNAME_"
        sleep 20
    ) | nc "$HOST" "$PORT" > /dev/null 2>&1 &
    PIDS="$PIDS $!"
done

cleanup() {
    for p in $PIDS ; do
        kill -9 "$p" 2>/dev/null
    done
}
trap cleanup EXIT

#   Let the holders be accepted and reach their read
sleep 2

#
#   The ordinary request. A fiber-exhausted agent closes it without a response, which curl reports
#   as an empty status code rather than as any HTTP error.
#
code=$(curl_ -s -o /dev/null -m 5 -w '%{http_code}' "${ENDPOINT}/index.html" 2>/dev/null)
if [ "$code" = "200" ] ; then
    echo "✓ an ordinary request was served with ${HOLDERS} connections held mid-request"
else
    echo "✗ an ordinary request answered '${code}' with only ${HOLDERS} connections held;"
    echo "      the fiber pool, not web.limits.connections, is the binding ceiling"
    status=1
fi

cleanup
trap - EXIT

#   And the agent is still healthy once the holders go away
sleep 1
if alive ; then
    echo "✓ the agent recovered after the holders closed"
else
    echo "✗ the agent did not recover after the holders closed"
    status=1
fi

exit $status
