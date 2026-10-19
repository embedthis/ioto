#!/usr/bin/env bash
#   On a FAT document root, respelled requests must neither bypass protected routes nor poison subsequent
#   canonical lookups. Requires a scratch macOS disk image; skip when mounting is unavailable.
set -u

PORT=4977
TMP=$(mktemp -d "${TMPDIR:-/tmp}/fatspell.XXXXXX")
IMG="$TMP/fat.dmg"
MNT="$TMP/mnt"
CONFIG="$TMP/web.json5"
LOG="$TMP/web.log"
FAIL=0

command -v web >/dev/null 2>&1 || { echo "SKIP: the web command is not on PATH" ; exit 77 ; }
command -v hdiutil >/dev/null 2>&1 || { echo "SKIP: needs hdiutil (macOS) to mount a FAT volume" ; exit 77 ; }

mkdir -p "$MNT" "$TMP/tmp"
chmod 700 "$TMP/tmp"
hdiutil create -size 40m -fs "MS-DOS FAT32" -volname FATSPELL -ov "$IMG" >/dev/null 2>&1 || {
    echo "SKIP: cannot create a FAT image" ; exit 77 ; }

mountfat() {
    hdiutil attach -nobrowse -mountpoint "$MNT" "$IMG" >/dev/null 2>&1
}
cleanup() {
    [ -n "${PID:-}" ] && kill -9 "$PID" 2>/dev/null
    hdiutil detach "$MNT" >/dev/null 2>&1
    rm -rf "$TMP"
}
trap cleanup EXIT

mountfat || { echo "SKIP: hdiutil cannot attach a disk image here" ; exit 77 ; }
GERE=$(printf 'g\xc3\xa9r\xc3\xa9')
mkdir -p "$MNT/site/protected" "$MNT/site/$GERE"
echo protected-body > "$MNT/site/protected/secret.txt"
echo gere-body > "$MNT/site/$GERE/s.txt"
echo public-body > "$MNT/site/index.html"
hdiutil detach "$MNT" >/dev/null 2>&1

cat > "$CONFIG" <<EOF
{
    web: {
        documents: '$MNT/site',
        index: 'index.html',
        listen: [ 'http://127.0.0.1:${PORT}' ],
        auth: {
            realm: 'Test Realm', authType: 'digest', algorithm: 'SHA-256',
            secret: 'test-secret-1234567890abcdef',
            roles: { admin: ['all'] },
            users: {
                alice: { password: 'SHA256:11c66702489123a02c7dd0860e47fc4989bb0805ba5835fbbb36f316ec83eb83', role: 'admin' },
            },
        },
        routes: [
            { match: '/protected/', role: 'admin', authType: 'digest' },
            { match: '/${GERE}/', role: 'admin', authType: 'digest' },
            { },
        ],
        upload: { dir: '$TMP/tmp' },
    },
}
EOF

start() {
    mountfat || { echo "SKIP: cannot remount the FAT image" ; exit 77 ; }
    web --config "$CONFIG" --quiet > "$LOG" 2>&1 &
    PID=$!
    sleep 1
}
stop() {
    kill -9 "$PID" 2>/dev/null
    wait "$PID" 2>/dev/null
    PID=
    hdiutil detach "$MNT" >/dev/null 2>&1
}
expect() {
    local want=$1 ; shift
    local got
    got=$(curl -s -m 5 --path-as-is -o /dev/null -w '%{http_code}' "$@")
    if [ "$got" != "$want" ] ; then
        echo "FAIL: expected $want, got $got: $*"
        FAIL=1
    else
        echo "ok: $want $*"
    fi
}
B=http://127.0.0.1:${PORT}

#   1. Unauthenticated Unicode case variant of a protected route, on a cold cache
start
expect 401 "$B/g%C3%A9r%C3%A9/s.txt"
expect 404 "$B/G%C3%89R%C3%89/s.txt"
stop

#   2. A re-spelled request must not deny the canonical spelling to later requests
start
expect 404 "$B/INDEX.HTML"
expect 200 "$B/index.html"
curl -s -m 5 -o /dev/null "$B/PROTECTED"     # names the protected directory; the status is not under test here
expect 200 --digest -u alice:password "$B/protected/secret.txt"
stop

exit $FAIL
