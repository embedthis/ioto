#!/usr/bin/env bash
#
#   nowebsock.tst.sh - The client builds without WebSocket support and refuses a WebSocket request
#
#   Compiles urlLib.c and the url command with ME_COM_WEBSOCK=0, warnings as errors, then requests a
#   ws:// URL from the test server. The request must fail with an error, never be sent as plain HTTP.
#
set -u

SRC=../src
BIN=../build/bin
DIR=tmp/nowebsock
CC=${CC:-cc}

case "$(uname -s)" in
    Darwin) OS=macosx ;;
    Linux) OS=linux ;;
    *) echo "SKIP: scratch builds are only scripted for macOS and Linux" ; exit 77 ;;
esac
RSP=../projects/gmake2/tls-${OS}.rsp
[ -f "$RSP" ] || { echo "SKIP: $RSP not generated" ; exit 77 ; }
command -v "$CC" >/dev/null 2>&1 || { echo "SKIP: no C compiler" ; exit 77 ; }

rm -fr "$DIR"
mkdir -p "$DIR"

if ! "$CC" -O0 -Wall -Werror -DME_VERSION='"0.0.0"' -DME_DEBUG=1 -DME_COM_WEBSOCK=0 @"$RSP" \
        -I$SRC -I$SRC/osdep -I$SRC/r -I$SRC/uctx -I$SRC/json -I$SRC/crypt -I$SRC/websock \
        -o "$DIR/url" $SRC/mains/url.c $SRC/urlLib.c \
        $BIN/libjson.a $BIN/libcrypt.a $BIN/libr.a $BIN/libuctx.a -lm -lpthread -ldl > "$DIR/build.log" 2>&1 ; then
    cat "$DIR/build.log"
    echo "✗ url does not compile with ME_COM_WEBSOCK=0"
    exit 1
fi
echo "✓ url compiles with ME_COM_WEBSOCK=0"

ENDPOINT=$(json web.listen[0] web.json5)
WS=$(echo "$ENDPOINT" | sed 's/^http/ws/')

OUT=$("$DIR/url" --timeout 10 "$WS/index.html" 2>&1)
STATUS=$?
if [ $STATUS -eq 0 ] ; then
    echo "✗ a ws:// request succeeded without WebSocket support: $OUT"
    exit 1
fi
if ! echo "$OUT" | grep -q "WebSocket support is not enabled" ; then
    echo "✗ expected the WebSocket support error, got: $OUT"
    exit 1
fi
echo "✓ a ws:// request is refused: WebSocket support is not enabled"
exit 0
