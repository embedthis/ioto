#!/usr/bin/env bash
#   An occupied listener must make the web command exit non-zero so supervisors detect startup failure.
set -u

PORT=4975
DIR=tmp/start-fail

command -v web >/dev/null 2>&1 || { echo "SKIP: the web command is not on PATH" ; exit 77 ; }
#   Resolve before changing directory: a relative PATH entry (../build/bin) would not survive the cd
WEB=$(cd "$(dirname "$(command -v web)")" && pwd)/web
command -v python3 >/dev/null 2>&1 || { echo "SKIP: python3 is needed to hold the port" ; exit 77 ; }
rm -fr "$DIR"
mkdir -p "$DIR/web"

cat > "$DIR/web.json5" <<CFG
{
    web: {
        documents: 'web',
        listen: ['http://127.0.0.1:${PORT}'],
        routes: [ { match: '', handler: 'file' } ],
    },
}
CFG

#   Hold the port so the server cannot bind it
python3 -c "
import socket, time
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(('127.0.0.1', ${PORT}))
s.listen(1)
time.sleep(30)
" &
HOLDER=$!
trap 'kill $HOLDER 2>/dev/null ; wait $HOLDER 2>/dev/null' EXIT

for _ in $(seq 1 50) ; do
    python3 -c "import socket; socket.create_connection(('127.0.0.1', ${PORT}), 0.2)" 2>/dev/null && break
    sleep 0.1
done

status=0
LOG="$DIR/web.log"
(cd "$DIR" && exec "$WEB" --config web.json5 --quiet) > "$LOG" 2>&1 &
PID=$!
for _ in $(seq 1 50) ; do
    kill -0 $PID 2>/dev/null || break
    sleep 0.1
done
if kill -0 $PID 2>/dev/null ; then
    kill $PID 2>/dev/null
    wait $PID 2>/dev/null
    echo "✗ the server kept running although its port was taken"
    cat "$LOG"
    status=1
else
    wait $PID
    rc=$?
    if [ $rc -eq 0 ] ; then
        echo "✗ the server could not start its host but exited with status 0"
        cat "$LOG"
        status=1
    else
        echo "✓ the server could not start its host and exited ${rc}"
    fi
fi

rm -fr "$DIR"
exit $status
