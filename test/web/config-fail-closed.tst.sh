#!/usr/bin/env bash
#   Missing or malformed configuration must fail startup with the filename, never install a public fallback
#   route.
set -u

PORT=4974
DIR=tmp/config-fail-closed

command -v web >/dev/null 2>&1 || { echo "SKIP: the web command is not on PATH" ; exit 77 ; }
rm -fr "$DIR"
mkdir -p "$DIR/web"
echo protected > "$DIR/web/private.html"

#   One missing comma after the routes array
cat > "$DIR/broken.json5" <<CFG
{
    web: {
        documents: 'web',
        auth: { roles: { user: [], admin: ['user'] } },
        routes: [ { match: '', role: 'admin' } ]
        upload: { dir: 'tmp' },
    },
}
CFG

status=0
for cfg in broken.json5 missing.json5 ; do
    LOG="$DIR/$cfg.log"
    (cd "$DIR" && exec web --config "$cfg" --listen "127.0.0.1:${PORT}" --quiet) > "$LOG" 2>&1 &
    PID=$!

    for _ in $(seq 1 50) ; do
        kill -0 $PID 2>/dev/null || break
        sleep 0.1
    done
    if kill -0 $PID 2>/dev/null ; then
        code=$(curl -s -m 2 -o /dev/null -w '%{http_code}' "http://127.0.0.1:${PORT}/private.html" 2>/dev/null)
        kill $PID 2>/dev/null
        wait $PID 2>/dev/null
        echo "✗ ${cfg}: the server started (GET /private.html -> ${code})"
        status=1
        continue
    fi
    wait $PID
    rc=$?
    if [ $rc -eq 0 ] ; then
        echo "✗ ${cfg}: the server exited with status 0"
        status=1
    elif ! grep -q "Cannot load config file \"${cfg}\"" "$LOG" ; then
        echo "✗ ${cfg}: the server exited ${rc} without naming the config file"
        cat "$LOG"
        status=1
    else
        echo "✓ ${cfg}: the server refused to start (exit ${rc})"
    fi
done

rm -fr "$DIR"
exit $status
