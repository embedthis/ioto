#!/usr/bin/env bash
#
#   background.tst.sh - The agent started with --background serves, and keeps serving across a restart
#
#   The http app is copied to a scratch tree and started with --background, so the launching process
#   exits and a daemon keeps running. The daemon must answer 200 for its index, restart on SIGUSR1 and
#   answer 200 again, and log no event-notifier errors. Daemonizing after the runtime is initialized
#   leaves the daemon with a kqueue descriptor that did not survive the fork, so on macOS and BSD
#   every request goes unanswered.
#
#   The agent writes a pid file only when run as root, so the daemon is found by its unique --home
#   argument and signalled by that pid alone.
#
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BIN="$ROOT/build/bin"
SCRATCH="$(pwd)/.testme/background"
DIR="$SCRATCH/apps/http"
PORT=4465
URL="http://127.0.0.1:${PORT}/index.html"

if ! "$ROOT/test/utils/skip-windows.sh" ; then
    exit 77
fi
. "$ROOT/test/utils/app-copy.sh"

status=0
PID=

fail() {
    echo "✗ $*" >&2
    status=1
}

daemonPid() {
    ps -eo pid=,args= | awk -v home="--home $DIR" 'index($0, home) && !/awk/ { print $1; exit }'
}

stopDaemon() {
    local i

    [ -n "$PID" ] || PID=$(daemonPid)
    if [ -n "$PID" ] ; then
        kill -TERM "$PID" 2>/dev/null
        for i in $(seq 1 20) ; do
            kill -0 "$PID" 2>/dev/null || break
            sleep 0.2
        done
        kill -9 "$PID" 2>/dev/null
        PID=
    fi
}
trap stopDaemon EXIT

#   Poll until the daemon answers 200, or give up after about ten seconds
waitFor200() {
    local code i

    for i in $(seq 1 50) ; do
        code=$(curl -s -o /dev/null -w '%{http_code}' --max-time 1 "$URL" 2>/dev/null)
        if [ "$code" = "200" ] ; then
            return 0
        fi
        sleep 0.2
    done
    echo "$code"
    return 1
}

if [ ! -x "$BIN/ioto-http" ] ; then
    echo "✗ ioto-http is not built" >&2
    exit 1
fi
if curl -s -o /dev/null --max-time 1 "$URL" 2>/dev/null ; then
    echo "✗ something already answers on port $PORT" >&2
    exit 1
fi
copyApp http "$SCRATCH" || exit 1

"$BIN/ioto-http" --background --home "$DIR" --listen "['http://127.0.0.1:${PORT}']" > "$SCRATCH/ioto.out" 2>&1
rc=$?
if [ $rc -ne 0 ] ; then
    fail "ioto-http --background exited with status $rc"
fi

if code=$(waitFor200) ; then
    echo "✓ The backgrounded agent answers 200"
else
    fail "the backgrounded agent answered ${code:-000}, expected 200"
fi

PID=$(daemonPid)
if [ -z "$PID" ] ; then
    fail "no daemon is running"
else
    kill -USR1 "$PID"
    sleep 1
    if ! kill -0 "$PID" 2>/dev/null ; then
        fail "the daemon exited on restart"
    elif code=$(waitFor200) ; then
        echo "✓ The backgrounded agent answers 200 after a restart"
    else
        fail "after a restart the backgrounded agent answered ${code:-000}, expected 200"
    fi
fi
stopDaemon

if grep -q "kevent\|epoll" "$SCRATCH/ioto.out" ; then
    fail "the daemon logged event notifier errors"
fi
if [ $status -ne 0 ] ; then
    sed -n '1,40p' "$SCRATCH/ioto.out" >&2
else
    rm -rf "$SCRATCH"
fi
exit $status
