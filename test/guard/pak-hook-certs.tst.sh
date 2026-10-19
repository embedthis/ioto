#!/usr/bin/env bash
#   pak sync must refresh published certificate files while preserving generated keys and certificate/key pairs.
#   Exercise the hook in a scratch tree without network access.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/pak-hook-certs.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT
status=0

mkdir -p "$TMP/paks/certs/dist" "$TMP/certs" "$TMP/lib" "$TMP/include"
for f in roots.crt aws.crt test.ans openssl.conf Makefile test.crt test.key ec.crt ec.key ; do
    echo "new $f" >"$TMP/paks/certs/dist/$f"
done
for f in roots.crt aws.crt test.ans openssl.conf Makefile test.crt test.key ; do
    echo "old $f" >"$TMP/certs/$f"
done

(cd "$TMP" && bash "$ROOT/.pak-hook.sh" install certs) || { echo "✗ the hook failed" ; exit 1 ; }

expect() {
    local file="$1" want="$2" why="$3"
    if [ "$(cat "$TMP/certs/$file" 2>/dev/null)" != "$want $file" ] ; then
        echo "✗ certs/$file is \"$(cat "$TMP/certs/$file" 2>/dev/null)\": $why"
        status=1
    else
        echo "✓ certs/$file: $why"
    fi
}

for f in roots.crt aws.crt test.ans openssl.conf Makefile ; do
    expect "$f" new "a published file is replaced on every sync"
done
for f in test.crt test.key ; do
    expect "$f" old "generated material already present is kept"
done
for f in ec.crt ec.key ; do
    expect "$f" new "generated material not yet present is installed"
done
exit $status
