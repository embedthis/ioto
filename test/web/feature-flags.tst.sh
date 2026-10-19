#!/usr/bin/env bash
#   Compile out TLS, Digest, HTTP auth, sessions and uploads in scratch builds. Configurations or requests
#   requiring missing features must fail without downgrade.
set -u

PORT=4979
DIR=tmp/feature-flags
SRC=../src

CC=${CC:-cc}
[ -f "$SRC/mains/web.c" ] || { echo "SKIP: needs the web module source tree" ; exit 77 ; }
command -v "$CC" >/dev/null 2>&1 || { echo "SKIP: no C compiler" ; exit 77 ; }
command -v curl >/dev/null 2>&1 || { echo "SKIP: curl is not on PATH" ; exit 77 ; }
case "$(uname -s)" in
    Darwin|Linux) ;;
    *) echo "SKIP: scratch builds are only scripted for macOS and Linux" ; exit 77 ;;
esac

rm -fr "$DIR"
mkdir -p "$DIR/site/admin"
echo secret > "$DIR/site/admin/index.html"

#   buildWeb out flags...
buildWeb() {
    local out=$1
    shift
    "$CC" -O0 -w -DME_VERSION='"0.0.0"' -DME_DEBUG=1 -DME_WEB_TEST_ACTIONS=0 "$@" \
        -I$SRC -I$SRC/osdep -I$SRC/r -I$SRC/uctx -I$SRC/json -I$SRC/crypt -I$SRC/url -I$SRC/websock \
        -I/opt/homebrew/include -L/opt/homebrew/lib -o "$out" $SRC/mains/web.c $SRC/*.c \
        $SRC/r/rLib.c $SRC/uctx/uctxLib.c $SRC/uctx/uctxAssembly.S $SRC/json/jsonLib.c \
        $SRC/crypt/cryptLib.c $SRC/url/urlLib.c $SRC/websock/websockLib.c \
        -lm -lpthread -ldl -lssl -lcrypto > "$out.build.log" 2>&1
}

#   writeConfig file listen authType
writeConfig() {
    cat > "$1" <<CFG
{
    log: { path: 'stdout', types: 'error,info', sources: 'all' },
    web: {
        documents: 'site',
        listen: ['$2'],
        auth: { roles: { admin: [] }, users: { alice: { password: 'BF1:00128:abcdefghijklmnopqrstuv:abcdefghijklmnopqrstuvwxyzabcd', role: 'admin' } } },
        upload: { dir: '.' },
        routes: [ { match: '/admin/', handler: 'file', role: 'admin', authType: '$3' }, { match: '', handler: 'file' } ],
    },
}
CFG
}

#   expectRefused name config probeUrl
expectRefused() {
    local name=$1 cfg=$2 url=$3 code hdr PID rc
    (cd "$DIR" && exec ./$name --config "$cfg" --quiet) > "$DIR/$name.log" 2>&1 &
    PID=$!
    for _ in $(seq 1 30) ; do
        kill -0 $PID 2>/dev/null || break
        sleep 0.1
    done
    if kill -0 $PID 2>/dev/null ; then
        code=$(curl -s -m 2 -o /dev/null -w '%{http_code}' "$url" 2>/dev/null)
        hdr=$(curl -s -m 2 -i "$url" 2>/dev/null | grep -i '^WWW-Authenticate:' | tr -d '\r')
        kill $PID 2>/dev/null
        wait $PID 2>/dev/null
        echo "✗ $name: the server started (GET $url -> $code $hdr)"
        cat "$DIR/$name.log"
        return 1
    fi
    wait $PID
    rc=$?
    #   A refused start must exit non-zero so a supervisor sees the failure
    if [ $rc -eq 0 ] ; then
        echo "✗ $name: the server refused to start but exited with status 0"
        cat "$DIR/$name.log"
        return 1
    fi
    echo "✓ $name: the server refused to start (exit $rc)"
    return 0
}

status=0

if ! buildWeb "$DIR/web-nossl" -DME_COM_SSL=0 -DME_COM_OPENSSL=0 -DME_COM_MBEDTLS=0 ; then
    echo "✗ ME_COM_SSL=0 build failed" ; cat "$DIR/web-nossl.build.log" ; exit 1
fi
writeConfig "$DIR/nossl.json5" "https://127.0.0.1:$PORT" basic
expectRefused web-nossl nossl.json5 "http://127.0.0.1:$PORT/" || status=1

if ! buildWeb "$DIR/web-nodigest" -DME_WEB_AUTH_DIGEST=0 -DME_COM_SSL=1 -DME_COM_OPENSSL=1 -DME_COM_MBEDTLS=0 ; then
    echo "✗ ME_WEB_AUTH_DIGEST=0 build failed" ; cat "$DIR/web-nodigest.build.log" ; exit 1
fi
writeConfig "$DIR/nodigest.json5" "http://127.0.0.1:$PORT" digest
expectRefused web-nodigest nodigest.json5 "http://127.0.0.1:$PORT/admin/index.html" || status=1

#   Every feature flag builds in its OFF state, and a configuration needing the feature refuses
SSL="-DME_COM_SSL=1 -DME_COM_OPENSSL=1 -DME_COM_MBEDTLS=0"

if ! buildWeb "$DIR/web-noauth" -DME_WEB_HTTP_AUTH=0 $SSL ; then
    echo "✗ ME_WEB_HTTP_AUTH=0 build failed" ; cat "$DIR/web-noauth.build.log" ; status=1
else
    writeConfig "$DIR/noauth.json5" "http://127.0.0.1:$PORT" basic
    expectRefused web-noauth noauth.json5 "http://127.0.0.1:$PORT/admin/index.html" || status=1
fi

# HTTP authentication requires bcrypt; reject explicitly incompatible feature flags.
if ! buildWeb "$DIR/web-nobcrypt" -DME_CRYPT_BCRYPT=0 $SSL ; then
    echo "✗ ME_CRYPT_BCRYPT=0 build failed" ; cat "$DIR/web-nobcrypt.build.log" ; status=1
else
    echo "✓ web-nobcrypt: builds without bcrypt"
    writeConfig "$DIR/nobcrypt.json5" "http://127.0.0.1:$PORT" basic
    expectRefused web-nobcrypt nobcrypt.json5 "http://127.0.0.1:$PORT/admin/index.html" || status=1
fi

if buildWeb "$DIR/web-authnobcrypt" -DME_CRYPT_BCRYPT=0 -DME_WEB_HTTP_AUTH=1 $SSL ; then
    echo "✗ web-authnobcrypt: HTTP authentication built without bcrypt" ; status=1
elif ! grep -q ME_CRYPT_BCRYPT "$DIR/web-authnobcrypt.build.log" ; then
    echo "✗ web-authnobcrypt: the build failed without naming the missing flag"
    head -20 "$DIR/web-authnobcrypt.build.log" ; status=1
else
    echo "✓ web-authnobcrypt: refused with an #error naming ME_CRYPT_BCRYPT"
fi

if ! buildWeb "$DIR/web-nosessions" -DME_WEB_SESSIONS=0 $SSL ; then
    echo "✗ ME_WEB_SESSIONS=0 build failed" ; cat "$DIR/web-nosessions.build.log" ; status=1
else
    cat > "$DIR/nosessions.json5" <<CFG
{
    log: { path: 'stdout', types: 'error,info', sources: 'all' },
    web: { documents: 'site', listen: ['http://127.0.0.1:$PORT'], upload: { dir: '.' },
           routes: [ { match: '/admin/', handler: 'file', xsrf: true }, { match: '', handler: 'file' } ] },
}
CFG
    expectRefused web-nosessions nosessions.json5 "http://127.0.0.1:$PORT/admin/index.html" || status=1
fi

if ! buildWeb "$DIR/web-noupload" -DME_WEB_UPLOAD=0 $SSL ; then
    echo "✗ ME_WEB_UPLOAD=0 build failed" ; cat "$DIR/web-noupload.build.log" ; status=1
else
    cat > "$DIR/noupload.json5" <<CFG
{
    log: { path: 'stdout', types: 'error', sources: 'all' },
    web: { documents: 'site', listen: ['http://127.0.0.1:$PORT'], routes: [ { match: '', handler: 'file', methods: ['GET', 'POST'] } ] },
}
CFG
    (cd "$DIR" && exec ./web-noupload --config noupload.json5 --quiet) > "$DIR/web-noupload.log" 2>&1 &
    PID=$!
    for _ in $(seq 1 30) ; do
        curl -s -o /dev/null --max-time 1 "http://127.0.0.1:$PORT/admin/index.html" && break
        sleep 0.1
    done
    code=$(curl -s -o /dev/null -w '%{http_code}' --max-time 5 -F "file=@$DIR/site/admin/index.html" \
        "http://127.0.0.1:$PORT/admin/index.html")
    kill $PID 2>/dev/null
    wait $PID 2>/dev/null
    if [ "$code" = "415" ] ; then
        echo "✓ web-noupload: a multipart upload is refused (415)"
    else
        echo "✗ web-noupload: a multipart upload got $code" ; cat "$DIR/web-noupload.log" ; status=1
    fi
fi

[ -n "${KEEP:-}" ] || rm -fr "$DIR"
exit $status
