#!/usr/bin/env bash
#
#   body-reread.tst.sh - A second webReadBody must not re-parse and orphan the first parse
#
#   web->chunked is a state-machine position, not a boolean, and its terminal value WEB_CHUNK_EOF is
#   truthy, so the "no body remains" guard fell through for a fully-read chunked request. The second
#   call re-parsed the same body and assigned over web->vars, orphaning the whole first parse. A POST
#   to a directory path without a trailing slash reaches it with no configuration at all: the file
#   handler's redirect calls webReadBody again after handleRequest already has.
#
#   The leak is invisible in the response - the redirect is correct either way - so this measures the
#   server's resident size across a batch of such requests. The unfixed server grows by roughly the
#   parsed size of every body it is sent; the fixed one does not grow.
#

set -u

PORT=$(json 'web.listen[0]' web.json5 | sed 's/.*://')
REQUESTS=400
#   The leak is ~20KB per request here, so ~8MB over the batch. Well clear of allocator noise.
THRESHOLD_KB=3000
BATCH=tmp/body-reread.req

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

#
#   A pipelined batch of chunked POSTs to a directory path, which the file handler answers with a
#   301 to the slashed form. The last one closes the connection so the reader returns at once.
#
awk -v count=$REQUESTS 'BEGIN {
    body = "{"
    for (i = 0; i < 200; i++) {
        v = ""
        for (j = 0; j < 64; j++) v = v "v"
        body = body (i ? "," : "") "\"k" i "\":\"" v "\""
    }
    body = body "}"
    len = sprintf("%x", length(body))
    for (n = 0; n < count; n++) {
        close_hdr = (n == count - 1) ? "Connection: close\r\n" : ""
        printf "POST /upload HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\nTransfer-Encoding: chunked\r\n%s\r\n%s\r\n%s\r\n0\r\n\r\n", close_hdr, len, body
    }
}' > $BATCH

#   Warm up so the sample is not measuring first-touch allocation
nc 127.0.0.1 ${PORT} < $BATCH > /dev/null 2>&1
BEFORE=$(rss)

nc 127.0.0.1 ${PORT} < $BATCH > /dev/null 2>&1
AFTER=$(rss)
rm -f $BATCH

if [ -z "$BEFORE" ] || [ -z "$AFTER" ] ; then
    echo "✗ cannot sample the resident size of pid ${PID}"
    exit 1
fi
GROWTH=$((AFTER - BEFORE))
echo "resident size ${BEFORE}KB -> ${AFTER}KB over ${REQUESTS} chunked redirect requests (${GROWTH}KB)"

if [ $GROWTH -gt $THRESHOLD_KB ] ; then
    echo "✗ the server grew ${GROWTH}KB, over the ${THRESHOLD_KB}KB threshold - the body parse is being orphaned"
    exit 1
fi
echo "✓ a re-read of a chunked body does not orphan the parse"
exit 0
