#!/usr/bin/env bash
#
#   websocket-upgrade-guard.tst.sh - An Upgrade header the server did not act on must not kill it
#
#   echoAction (apps/unit/src/unit.c) gated on web->upgrade, which is the Upgrade header's *value*.
#   The header parser sets it for any Upgrade at all, while handleRequest performs the upgrade only
#   for "websocket" and allocates web->webSocket only there. So "Upgrade: h2c", "Upgrade: x" and an
#   empty "Upgrade:" all passed the guard with web->webSocket still null, and webSocketRun stored
#   through the null pointer. One 37-byte unauthenticated request terminated the process, taking
#   every other in-flight request and established WebSocket session with it. The fix is
#   web->upgraded, which is set only after the upgrade completes.
#
#   The liveness assertion is the half that matters. A handler that dies produces a dropped
#   connection, and a status-code check alone reads that as a transport error rather than as the
#   process death it is - so the test would "fail" for the wrong reason on the unfixed build and
#   pass for the wrong reason if the harness were lenient. Each case therefore ends by asking the
#   server for an ordinary page.
#
set -u

ENDPOINT="${TM_HTTP:-http://localhost:4480}"
ROUTE="/api/public/echo"
status=0

#
#   --noproxy: the endpoint is loopback, and a proxy in the environment must not be consulted for
#   it. A sandbox or corporate http_proxy otherwise intercepts these requests and the test measures
#   the proxy rather than the server.
#
#   A function, not a variable: an unquoted "$CURL" would glob the "*" against the directory.
curl_() {
    curl --noproxy '*' "$@"
}

#
#   A status code, not curl's exit status: the agent answers the probe with 200 but closes short of
#   the declared length, so "curl -f" reports error 18 for a server that is plainly alive. What is
#   being asked here is whether the process is still serving, and a status line is the answer to
#   that. (The short transfer is a separate defect and is not what this test is about.)
#
alive() {
    [ "$(curl_ -s -o /dev/null -m 5 -w '%{http_code}' "${ENDPOINT}/index.html" 2>/dev/null)" = "200" ]
}

#
#   Wait for the group's agent rather than assuming it. agent-setup.sh starts ioto-unit and blocks,
#   and opening the database takes longer than binding the listeners, so a test that asks
#   immediately can arrive first.
#
for _ in $(seq 1 100) ; do
    alive && break
    sleep 0.1
done

if ! alive ; then
    echo "✗ the agent is not answering on ${ENDPOINT} within 10 seconds"
    exit 1
fi

#
#   Every Upgrade token the server does not act on, including the empty one - which is the clearest
#   statement of the bug, because the guard accepted a header that says nothing.
#
for value in "h2c" "x" "" ; do
    label=${value:-<empty>}

    code=$(curl_ -s -o /dev/null -m 5 -w '%{http_code}' \
        -H "Upgrade: ${value}" "${ENDPOINT}${ROUTE}" 2>/dev/null)

    if [ "$code" = "400" ] ; then
        echo "✓ Upgrade: ${label} was refused with 400"
    else
        echo "✗ Upgrade: ${label} answered '${code}', expected 400"
        status=1
    fi

    if alive ; then
        echo "✓ the agent is still serving after Upgrade: ${label}"
    else
        echo "✗ the agent died on Upgrade: ${label}"
        exit 1
    fi
done

#
#   The control: a request with no Upgrade header at all takes the same path to the same handler
#   and must be refused the same way, so a 400 above is the guard and not the header parser.
#
code=$(curl_ -s -o /dev/null -m 5 -w '%{http_code}' "${ENDPOINT}${ROUTE}" 2>/dev/null)
if [ "$code" = "400" ] ; then
    echo "✓ a plain request to ${ROUTE} was refused with 400"
else
    echo "✗ a plain request to ${ROUTE} answered '${code}', expected 400"
    status=1
fi

exit $status
