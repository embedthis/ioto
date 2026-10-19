#!/usr/bin/env bash
#
#   exit.tst.sh - The unit app's test action must actually stop the agent when exit=1
#
#   unitTestAction (apps/unit/src/unit.c) read count/delay/exit with webGetVar, which
#   only sees form/body variables. A GET has no body, so "?name=health&exit=1" always read exit as
#   its default "0" and rStop() was never called - the agent kept serving. name had a fallback to
#   webGetQueryVar; count, delay and exit did not.
#
#   This calls the exit endpoint the way the defect was reproduced - a session-authenticated
#   GET - and then requires both symptoms of "the agent kept running" to be gone: the pidfile's
#   process must exit on its own (not merely be killable) and the port must stop answering. Must
#   fail before the fix and pass after.
#
set -u

ENDPOINT="${TM_HTTPS:-https://localhost:4491}"
HTTP_ENDPOINT="${TM_HTTP:-http://localhost:4490}"
PIDFILE=.pidfile
COOKIES=$(mktemp 2>/dev/null || echo "./.exit-cookies.$$")
status=0

cleanup() {
    rm -f "$COOKIES"
}
trap cleanup EXIT

if [ ! -f "$PIDFILE" ] ; then
    echo "✗ $PIDFILE is missing; agent-setup.sh did not start the group's agent"
    exit 1
fi
PID=$(cat "$PIDFILE")

LOGIN_STATUS=$(curl -sk -o /dev/null -w '%{http_code}' -c "$COOKIES" -X POST \
    -d 'username=guest&password=scenario-fixture-pw' -H 'Content-Type: application/x-www-form-urlencoded' \
    "${ENDPOINT}/api/public/login")
if [ "$LOGIN_STATUS" != "302" ] ; then
    echo "✗ login as guest answered $LOGIN_STATUS, expected 302"
    exit 1
fi

TEST_STATUS=$(curl -sk -o /dev/null -w '%{http_code}' -b "$COOKIES" \
    "${ENDPOINT}/api/test?name=health&exit=1")
if [ "$TEST_STATUS" != "200" ] ; then
    echo "✗ /api/test?name=health&exit=1 answered $TEST_STATUS, expected 200"
    exit 1
fi

# Allow the event loop to finish writing the response before exit.
GONE=0
for i in $(seq 1 50) ; do
    if ! kill -0 "$PID" 2>/dev/null ; then
        GONE=1
        break
    fi
    sleep 0.1
done
if [ "$GONE" != "1" ] ; then
    echo "✗ pid $PID is still alive 5s after exit=1; the agent did not stop"
    kill -9 "$PID" 2>/dev/null
    status=1
else
    echo "✓ pid $PID exited after exit=1"
fi

# Check that no listener remains after exit.
FREE=0
for i in $(seq 1 20) ; do
    if ! curl -s -o /dev/null -m 1 "${HTTP_ENDPOINT}/index.html" 2>/dev/null ; then
        FREE=1
        break
    fi
    sleep 0.1
done
if [ "$FREE" != "1" ] ; then
    echo "✗ ${HTTP_ENDPOINT} still answers after the agent should have exited"
    status=1
else
    echo "✓ ${HTTP_ENDPOINT} no longer answers"
fi

exit $status
