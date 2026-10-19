#!/usr/bin/env bash
#
#   range-ifrange-leak.tst.sh - An If-Range mismatch must free the range list, not unlink it
#
#   RFC 7233 section 3.2 says a non-matching If-Range makes the server ignore the Range header and
#   serve the whole representation. pickRanges implemented "ignore" by setting web->ranges to NULL,
#   which orphaned every WebRange the request had allocated: webFreeRanges is the list's only owner
#   and by then had nothing to walk. Up to sixteen nodes per request, from two headers on a plain
#   GET of any file the server will serve.
#
#   The response is a correct 200 with the full body either way, so the growth is the only
#   observable. The control - the same Range header with no If-Range - never leaked.
#

set -u

PORT=$(json 'web.listen[0]' web.json5 | sed 's/.*://')
#   ~512 bytes per request, and maxRequests bounds one connection, so drive several batches
PER_CONNECTION=900
CONNECTIONS=24
THRESHOLD_KB=3000
BATCH=tmp/range-ifrange.req

case "$(uname -s)" in
Darwin|Linux) ;;
*)  echo "SKIP: resident size is only sampled on macOS and Linux" ; exit 77 ;;
esac

# Use ss when lsof is unavailable.
if command -v lsof >/dev/null 2>&1 ; then
    PID=$(lsof -ti tcp:${PORT} -sTCP:LISTEN 2>/dev/null | head -1)
else
    PID=$(ss -Hltnp "sport = :${PORT}" 2>/dev/null | sed -n 's/.*pid=\([0-9]*\).*/\1/p' | head -1)
fi
if [ -z "$PID" ] ; then
    echo "✗ cannot find the server listening on ${PORT}"
    exit 1
fi

rss() {
    ps -o rss= -p "$PID" 2>/dev/null | tr -d ' '
}

#   Sixteen ranges, the most the parser keeps, plus an If-Range that can never match the real ETag
awk -v count=$PER_CONNECTION 'BEGIN {
    ranges = ""
    for (i = 0; i < 16; i++) ranges = ranges (i ? "," : "") i "-" i
    for (n = 0; n < count; n++) {
        close_hdr = (n == count - 1) ? "Connection: close\r\n" : ""
        printf "GET /index.html HTTP/1.1\r\nHost: x\r\nRange: bytes=%s\r\nIf-Range: \"never-matches\"\r\n%s\r\n", ranges, close_hdr
    }
}' > $BATCH

drive() {
    i=0
    while [ $i -lt $CONNECTIONS ] ; do
        nc 127.0.0.1 ${PORT} < $BATCH > /dev/null 2>&1
        i=$((i + 1))
    done
}

#   Warm up so the sample is not measuring first-touch allocation
drive
BEFORE=$(rss)

drive
AFTER=$(rss)
rm -f $BATCH

if [ -z "$BEFORE" ] || [ -z "$AFTER" ] ; then
    echo "✗ cannot sample the resident size of pid ${PID}"
    exit 1
fi
GROWTH=$((AFTER - BEFORE))
TOTAL=$((PER_CONNECTION * CONNECTIONS))
echo "resident size ${BEFORE}KB -> ${AFTER}KB over ${TOTAL} If-Range mismatch requests (${GROWTH}KB)"

if [ $GROWTH -gt $THRESHOLD_KB ] ; then
    echo "✗ the server grew ${GROWTH}KB, over the ${THRESHOLD_KB}KB threshold - the range list is being orphaned"
    exit 1
fi
echo "✓ an If-Range mismatch frees the range list"
exit 0
