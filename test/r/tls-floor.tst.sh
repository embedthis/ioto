#!/usr/bin/env bash
#   Reject TLS versions below OpenSSL 3.0/MbedTLS 3.6.7 and accept supported headers. Override locations with
#   OPENSSL_OLD, OPENSSL_NEW and MBEDTLS_DIR.
here=$(cd "$(dirname "$0")" && pwd)
if [ -f "$here/../src/r.c" ]; then
    inc="-I$here/../src -I$here/../src/osdep -I$here/../src/uctx"
    openssl="$here/../src/openssl.c"
    mbedtls="$here/../src/mbedtls.c"
else
    #   Vendored into the agent, where both providers are in the runtime amalgamation
    inc="-I$here/../../include"
    openssl="$here/../../lib/rLib.c"
    mbedtls="$openssl"
fi
work="${TMPDIR:-/tmp}/r-tls-floor.$$"
cc="${CC:-cc}"
checked=0

fail() {
    echo "✗ $1"
    rm -rf "$work"
    exit 1
}

mkdir -p "$work"
base="-w -DR_USE_CONFIG=1 -DME_DEBUG=1 -DME_COM_SSL=1 $inc"

#   Compile one provider against one header directory. Succeeds if it compiles.
compiles() {
    "$cc" $base $2 -I"$1/include" -c "$3" -o "$work/out.o" 2>"$work/err.log"
}

#   Expect the #error from the floor, not an unrelated failure
refused() {
    grep -q "R requires" "$work/err.log"
}

check_openssl() {
    dir=$1
    want=$2
    [ -f "$dir/include/openssl/opensslv.h" ] || return 0
    checked=$((checked + 1))
    if compiles "$dir" "-DME_COM_OPENSSL=1 -DME_COM_MBEDTLS=0" "$openssl"; then
        [ "$want" = accept ] || fail "openssl.c compiled against $dir with no diagnostic"
    else
        [ "$want" = refuse ] || fail "openssl.c was refused against $dir: $(head -3 "$work/err.log")"
        refused || fail "openssl.c failed against $dir, but not on the version floor: $(head -3 "$work/err.log")"
    fi
}

check_mbedtls() {
    dir=$1
    [ -f "$dir/include/mbedtls/build_info.h" ] || return 0
    checked=$((checked + 1))
    number=$(sed -n 's/^#define MBEDTLS_VERSION_NUMBER *\(0x[0-9A-Fa-f]*\).*/\1/p' "$dir/include/mbedtls/build_info.h")
    if compiles "$dir" "-DME_COM_OPENSSL=0 -DME_COM_MBEDTLS=1" "$mbedtls"; then
        [ $((number)) -ge $((0x03060700)) ] || fail "mbedtls.c compiled against MbedTLS $number with no diagnostic"
    else
        [ $((number)) -lt $((0x03060700)) ] || fail "mbedtls.c was refused against MbedTLS $number: $(head -3 "$work/err.log")"
        refused || fail "mbedtls.c failed against MbedTLS $number, but not on the version floor: $(head -3 "$work/err.log")"
    fi
}

check_openssl "${OPENSSL_OLD:-/opt/homebrew/opt/openssl@1.1}" refuse
check_openssl "${OPENSSL_NEW:-/opt/homebrew/opt/openssl}" accept
for dir in ${MBEDTLS_DIR:-/opt/homebrew/opt/mbedtls /opt/homebrew/opt/mbedtls@3} ; do
    check_mbedtls "$dir"
done

rm -rf "$work"
if [ $checked -eq 0 ] ; then
    echo "Skipped: no TLS library headers found"
    exit 0
fi
echo "✓ TLS provider version floors enforced ($checked header sets checked)"
exit 0
