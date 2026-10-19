#!/usr/bin/env bash
#
#   test-actions.tst.sh - The /test action table is not reachable unless it was asked for
#
#   test.c's action table is unauthenticated by construction: webTestInit adds every action with a
#   null role, and webCan returns 1 immediately for a null role. It dumps request headers including
#   Authorization and Cookie (/test/show), terminates the process on demand (/test/crash/null), and
#   writes uploaded content relative to the working directory (/test/upload).
#
#   It used to be registered whenever ME_DEBUG was set, and the documented build command - plain
#   "make" - defaults to debug. So building the ordinary way and running this server published all
#   of that to any unauthenticated client, with nothing in the output, the --help or the log to say
#   so. ME_DEBUG was doing two unrelated jobs: "give me symbols and assertions", which a debug build
#   wants, and "compile in the test scaffolding", which it does not.
#
#   Compiling the table in is now separate (ME_WEB_TEST_ACTIONS) from registering it (--test-actions),
#   and this asserts the second half: a server started without the flag answers 404, on the same
#   binary the suite's own server registers those routes on.
#
#   The suite's server is started WITH --test-actions by setup.sh, so this starts its own on a
#   private port rather than testing the shared one.
#
set -u

PORT=4973
CONFIG=web.json5
LOG=tmp/test-actions.log

command -v web >/dev/null 2>&1 || { echo "SKIP: the web command is not on PATH" ; exit 77 ; }
mkdir -p tmp

web --config $CONFIG --listen $PORT --quiet > "$LOG" 2>&1 &
PID=$!
trap 'kill $PID 2>/dev/null' EXIT

for _ in $(seq 1 50) ; do
    if curl -fsS -o /dev/null "http://127.0.0.1:${PORT}/index.html" 2>/dev/null ; then
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
#   Every route webTestInit adds. A 404 is the pass: the route is not in the table.
#
for path in /test/show /test/bulk /test/recurse /test/upload ; do
    code=$(curl -s -o /dev/null -w '%{http_code}' "http://127.0.0.1:${PORT}${path}" 2>/dev/null)
    if [ "$code" = "404" ] ; then
        echo "✓ ${path} is not registered (404)"
    else
        echo "✗ ${path} answered ${code}, so the test actions are registered without --test-actions"
        status=1
    fi
done

#
#   The liveness half. /test/crash/null is a deliberate null dereference, so a server that still
#   has it registered dies here rather than answering - and a status-code assertion alone would
#   read that dropped connection as an error rather than as the exposure it is.
#
curl -s -o /dev/null -m 5 "http://127.0.0.1:${PORT}/test/crash/null" 2>/dev/null
sleep 0.3
if kill -0 $PID 2>/dev/null ; then
    echo "✓ the server survived /test/crash/null"
else
    echo "✗ /test/crash/null terminated the server, so the crash action is registered"
    status=1
fi

exit $status
