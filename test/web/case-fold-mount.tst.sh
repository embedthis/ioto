#!/usr/bin/env bash
#   Probe case folding inside the document root, independently of its parent. Use a scratch case-sensitive APFS
#   image under a folding parent; skip when hdiutil/mounting is unavailable.
set -u

PORT=4978
TMP=$(mktemp -d "${TMPDIR:-/tmp}/casefold.XXXXXX")
IMG="$TMP/cs.dmg"
DOCS="$TMP/docs"
CONFIG="$TMP/web.json5"
LOG="$TMP/web.log"
PID=
FAIL=0

command -v web >/dev/null 2>&1 || { echo "SKIP: the web command is not on PATH" ; exit 77 ; }
command -v hdiutil >/dev/null 2>&1 || { echo "SKIP: needs hdiutil (macOS) to mount a volume" ; exit 77 ; }

cleanup() {
    [ -n "${PID:-}" ] && kill -9 "$PID" 2>/dev/null
    hdiutil detach "$DOCS" >/dev/null 2>&1
    rm -rf "$TMP"
}
trap cleanup EXIT

#   The mirror image needs a case-folding parent
mkdir -p "$TMP/probe"
[ -d "$TMP/PROBE" ] || { echo "SKIP: the scratch directory's file system does not fold case" ; exit 77 ; }

mkdir -p "$DOCS" "$TMP/tmp"
chmod 700 "$TMP/tmp"
hdiutil create -size 40m -fs "Case-sensitive APFS" -volname CASEFOLD -ov "$IMG" >/dev/null 2>&1 || {
    echo "SKIP: cannot create a case-sensitive image" ; exit 77 ; }
hdiutil attach -nobrowse -mountpoint "$DOCS" "$IMG" >/dev/null 2>&1 || {
    echo "SKIP: hdiutil cannot attach a disk image here" ; exit 77 ; }

#   Distinct directories on a case-sensitive volume
mkdir -p "$DOCS/admin" "$DOCS/ADMIN"
echo admin-body > "$DOCS/admin/secret.txt"
echo public-body > "$DOCS/ADMIN/public.txt"
echo index-body > "$DOCS/index.html"

cat > "$CONFIG" <<EOF
{
    web: {
        documents: '$DOCS',
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
            { match: '/admin/', role: 'admin', authType: 'digest' },
            { },
        ],
        upload: { dir: '$TMP/tmp' },
    },
}
EOF

web --config "$CONFIG" --quiet > "$LOG" 2>&1 &
PID=$!
sleep 1

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

expect 200 "$B/index.html"
expect 401 "$B/admin/secret.txt"
#   The root is case-sensitive, so /ADMIN/ is a different, public directory
expect 200 "$B/ADMIN/public.txt"

exit $FAIL
