#!/usr/bin/env bash
#
#   log-path.tst.sh - log.path 'stdout' and 'stderr' mean the console whatever directories.log is
#
#   The blank app is copied to a scratch tree and given a local.json5 that sets directories.log to a
#   scratch directory and log.path to a console stream. The startup banner must appear on that stream
#   and no file of that name may be created in the log directory.
#
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BIN="$ROOT/build/bin"
SCRATCH="$(pwd)/.testme/log-path"
DIR="$SCRATCH/apps/blank"
LOGDIR="$SCRATCH/logs"
PORT=4466
BANNER="Starting Ioto"

. "$ROOT/test/utils/app-copy.sh"

status=0

fail() {
    echo "✗ $*" >&2
    status=1
}

if [ ! -x "$BIN/ioto-blank" ] ; then
    echo "✗ ioto-blank is not built" >&2
    exit 1
fi
copyApp blank "$SCRATCH" || exit 1

for stream in stdout stderr ; do
    rm -rf "$LOGDIR"
    mkdir -p "$LOGDIR"
    printf "{directories: {log: '%s'}, log: {path: '%s'}}\n" "$LOGDIR" "$stream" > "$DIR/state/config/local.json5"

    (cd "$DIR" && exec "$BIN/ioto-blank" --exit 2 --listen "['http://127.0.0.1:${PORT}']" \
        > "$SCRATCH/out.txt" 2> "$SCRATCH/err.txt") &
    pid=$!
    for i in $(seq 1 75) ; do
        kill -0 "$pid" 2>/dev/null || break
        sleep 0.2
    done
    if kill -0 "$pid" 2>/dev/null ; then
        kill -9 "$pid" 2>/dev/null
        fail "log.path '$stream': ioto-blank did not exit"
    fi
    wait "$pid" 2>/dev/null

    if [ "$stream" = "stdout" ] ; then
        console="$SCRATCH/out.txt"
    else
        console="$SCRATCH/err.txt"
    fi
    if grep -q "$BANNER" "$console" ; then
        echo "✓ log.path '$stream' writes the startup banner to $stream"
    else
        fail "log.path '$stream' did not write the startup banner to $stream"
        sed -n '1,20p' "$SCRATCH/out.txt" "$SCRATCH/err.txt" >&2
    fi
    if [ -e "$LOGDIR/$stream" ] ; then
        fail "log.path '$stream' created the file $LOGDIR/$stream"
    else
        echo "✓ log.path '$stream' creates no file in directories.log"
    fi
done

if [ $status -eq 0 ] ; then
    rm -rf "$SCRATCH"
fi
exit $status
