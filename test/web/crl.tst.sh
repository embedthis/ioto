#!/usr/bin/env bash
#   Verify client-certificate revocation: accept a good certificate, reject a revoked one, and fail startup on
#   an unreadable CRL.
set -u

PORT=4978
DIR=tmp/crl

case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*)
    #   Native openssl and web need Windows paths, and Git Bash would rewrite the "/CN=" subject as a path
    export MSYS_NO_PATHCONV=1
    nativePath() { cygpath -m "$1" ; } ;;
*)
    nativePath() { echo "$1" ; } ;;
esac
CERTS=$(nativePath "$(cd ../certs && pwd)")

command -v web >/dev/null 2>&1 || { echo "SKIP: the web command is not on PATH" ; exit 77 ; }
command -v openssl >/dev/null 2>&1 || { echo "SKIP: openssl is not on PATH" ; exit 77 ; }
#   Resolve before changing directory: a relative PATH entry (../build/bin) would not survive the cd
WEB=$(cd "$(dirname "$(command -v web)")" && pwd)/web

rm -fr "$DIR"
mkdir -p "$DIR/site"
echo hello > "$DIR/site/index.html"
cd "$DIR" || exit 1

#   A minimal CA database for issuing and revoking with the test CA
touch index.txt
echo 1000 > serial
echo 1000 > crlnumber
cat > ca.cnf <<CNF
[ ca ]
default_ca = CA_default
[ CA_default ]
database = index.txt
new_certs_dir = .
serial = serial
crlnumber = crlnumber
default_md = sha256
default_days = 30
default_crl_days = 30
policy = policy_any
x509_extensions = client_ext
[ policy_any ]
commonName = supplied
[ client_ext ]
basicConstraints = CA:FALSE
keyUsage = digitalSignature, keyEncipherment
extendedKeyUsage = clientAuth
CNF

issue() {
    openssl req -new -newkey rsa:2048 -nodes -keyout "$1.key" -subj "/CN=$1" -out "$1.csr" >/dev/null 2>&1 &&
    openssl ca -batch -config ca.cnf -cert "$CERTS/ca.crt" -keyfile "$CERTS/ca.key" -in "$1.csr" -out "$1.crt" \
        >/dev/null 2>&1
}
if ! issue good || ! issue revoked ; then
    echo "✗ cannot issue client certificates" ; exit 1
fi
openssl ca -config ca.cnf -cert "$CERTS/ca.crt" -keyfile "$CERTS/ca.key" -revoke revoked.crt >/dev/null 2>&1
openssl ca -config ca.cnf -cert "$CERTS/ca.crt" -keyfile "$CERTS/ca.key" -gencrl -out crl.pem >/dev/null 2>&1

#   writeConfig file revoke
writeConfig() {
    cat > "$1" <<CFG
{
    log: { path: 'stdout', types: 'error', sources: 'all' },
    tls: {
        certificate: '$CERTS/test.crt',
        key: '$CERTS/test.key',
        verify: { client: true, issuer: true, authority: '$CERTS/ca.crt', revoke: '$2' },
    },
    web: {
        documents: 'site',
        listen: ['https://127.0.0.1:$PORT'],
        routes: [ { match: '', handler: 'file' } ],
    },
}
CFG
}

status=0

#   An unreadable revocation list refuses startup
writeConfig missing.json5 "$(nativePath "$(pwd)")/no-such.crl"
"$WEB" --config missing.json5 > missing.log 2>&1 &
PID=$!
for _ in $(seq 1 30) ; do
    kill -0 $PID 2>/dev/null || break
    sleep 0.1
done
if kill -0 $PID 2>/dev/null ; then
    kill $PID 2>/dev/null ; wait $PID 2>/dev/null
    echo "✗ the server started with an unreadable revocation list" ; cat missing.log ; status=1
else
    wait $PID
    rc=$?
    if [ $rc -eq 0 ] ; then
        echo "✗ an unreadable revocation list exited with status 0" ; status=1
    else
        echo "✓ an unreadable revocation list refuses startup (exit $rc)"
    fi
fi

writeConfig web.json5 "$(nativePath "$(pwd)")/crl.pem"
"$WEB" --config web.json5 > web.log 2>&1 &
PID=$!
# Print client-certificate request status via openssl; Windows Schannel curl cannot load PEM credentials.
fetch() {
    printf 'GET /index.html HTTP/1.0\r\nHost: 127.0.0.1\r\n\r\n' |
        openssl s_client -connect "127.0.0.1:$PORT" -cert "$1.crt" -key "$1.key" -quiet 2>"$1.err" |
        head -1 | awk '{print $2}'
}

for _ in $(seq 1 50) ; do
    [ -n "$(fetch good)" ] && break
    sleep 0.1
done

code=$(fetch good)
if [ "$code" = "200" ] ; then
    echo "✓ a valid client certificate is accepted"
else
    echo "✗ a valid client certificate got '$code'" ; cat good.err web.log ; status=1
fi

code=$(fetch revoked)
if [ "$code" = "200" ] ; then
    echo "✗ a revoked client certificate was accepted" ; status=1
else
    echo "✓ a revoked client certificate is refused"
fi

kill $PID 2>/dev/null
wait $PID 2>/dev/null
cd - >/dev/null
[ -n "${KEEP:-}" ] || rm -fr "$DIR"
exit $status
