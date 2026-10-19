#!/usr/bin/env bash
#   Published TLS minimum versions must match the runtime compile guards.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
status=0

fail() {
    echo "✗ $*" >&2
    status=1
}

#   0x30000000L -> 3.0, 0x03060700 -> 3.6.7
hexToVersion() {
    local n=$(( $1 )) parts=$2
    if [ "$parts" = "openssl" ] ; then
        echo "$(( (n >> 28) & 0xF )).$(( (n >> 20) & 0xFF ))"
    else
        echo "$(( (n >> 24) & 0xFF )).$(( (n >> 16) & 0xFF )).$(( (n >> 8) & 0xFF ))"
    fi
}

OHEX=$(sed -n 's/^#if OPENSSL_VERSION_NUMBER < \(0x[0-9A-Fa-f]*\)L*$/\1/p' "$ROOT/lib/rLib.c" | head -1)
MHEX=$(sed -n 's/^ *#if MBEDTLS_VERSION_NUMBER < \(0x[0-9A-Fa-f]*\)$/\1/p' "$ROOT/lib/rLib.c" | head -1)
if [ -z "$OHEX" ] || [ -z "$MHEX" ] ; then
    echo "✗ cannot find the OpenSSL and MbedTLS version guards in lib/rLib.c" >&2
    exit 1
fi
OPENSSL=$(hexToVersion "$OHEX" openssl)
MBEDTLS=$(hexToVersion "$MHEX" mbedtls)

grep -q "^OpenSSL | $OPENSSL\$" "$ROOT/README.md" || fail "README.md does not state the OpenSSL floor $OPENSSL"
grep -q "^MbedTLS | $MBEDTLS," "$ROOT/README.md" || fail "README.md does not state the MbedTLS floor $MBEDTLS"

FACTS="$ROOT/doc/compliance/release-facts.md"
if [ -f "$FACTS" ] ; then
    grep -q "^| OpenSSL | build-time, minimum $OPENSSL |" "$FACTS" || fail "release-facts.md does not state the OpenSSL floor $OPENSSL"
    grep -q "^| MbedTLS | build-time, minimum $MBEDTLS |" "$FACTS" || fail "release-facts.md does not state the MbedTLS floor $MBEDTLS"
else
    echo "  [Skip] doc/compliance/release-facts.md is not present"
fi

SBOM="$ROOT/doc/compliance/sbom/sbom.json"
if [ -f "$SBOM" ] ; then
    python3 - "$SBOM" "$OPENSSL.0" "$MBEDTLS" <<'PY' || status=1
import json, sys
sbom, want = json.load(open(sys.argv[1])), {'openssl': sys.argv[2], 'mbedtls': sys.argv[3]}
for c in sbom.get('components', []):
    if c.get('name') in want:
        got = {p['name']: p['value'] for p in c.get('properties', [])}.get('embedthis:minimumVersion')
        if got != want[c['name']]:
            print(f"✗ sbom.json {c['name']} minimumVersion is {got}, the runtime requires {want[c['name']]}", file=sys.stderr)
            sys.exit(1)
        del want[c['name']]
if want:
    print(f"✗ sbom.json has no component for {', '.join(want)}", file=sys.stderr)
    sys.exit(1)
PY
else
    echo "  [Skip] doc/compliance/sbom/sbom.json is not present"
fi

if [ $status -eq 0 ] ; then
    echo "✓ README, release facts and SBOM state the runtime's TLS floors (OpenSSL $OPENSSL, MbedTLS $MBEDTLS)"
fi
exit $status
