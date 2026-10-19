#!/usr/bin/env bash
#   Require the ESP32 MbedTLS clock guard after provider includes. Preprocess each macro combination without
#   ESP-IDF.
here=$(cd "$(dirname "$0")" && pwd)
source="$here/../src/mbedtls.c"
#   Vendored into the agent, the provider is in the runtime amalgamation
[ -f "$source" ] || source="$here/../../lib/rLib.c"
work="${TMPDIR:-/tmp}/r-tls-time-guard.$$"
cc="${CC:-cc}"
checked=0

fail() {
    echo "✗ $1"
    rm -rf "$work"
    exit 1
}

mkdir -p "$work"

guard=$(awk '/#if +ESP32 +&& +!defined\(MBEDTLS_HAVE_TIME_DATE\)/,/#endif/' "$source")
[ -n "$guard" ] || fail "mbedtls.c has no check for MBEDTLS_HAVE_TIME_DATE on ESP32"
printf '%s\n' "$guard" > "$work/guard.c"

#   The macro comes from the MbedTLS configuration, so the check is only meaningful after it is included
include=$(grep -n '#include "mbedtls/check_config.h"' "$source" | head -1 | cut -d: -f1)
check=$(grep -n 'ESP32 && !defined(MBEDTLS_HAVE_TIME_DATE)' "$source" | head -1 | cut -d: -f1)
[ -n "$include" ] && [ -n "$check" ] && [ "$check" -gt "$include" ] ||
    fail "the clock check must follow the MbedTLS configuration includes"
checked=$((checked + 1))

#   ESP32 without a clock is refused, with the reason
if "$cc" -E -DESP32=1 "$work/guard.c" >/dev/null 2>"$work/err.log"; then
    fail "ESP32 without MBEDTLS_HAVE_TIME_DATE was accepted"
fi
grep -q "R requires MBEDTLS_HAVE_TIME_DATE" "$work/err.log" ||
    fail "the build failed, but not on the clock check: $(head -3 "$work/err.log")"
checked=$((checked + 1))

#   ESP32 with a clock is accepted
"$cc" -E -DESP32=1 -DMBEDTLS_HAVE_TIME_DATE "$work/guard.c" >/dev/null 2>"$work/err.log" ||
    fail "ESP32 with MBEDTLS_HAVE_TIME_DATE was refused: $(head -3 "$work/err.log")"
checked=$((checked + 1))

#   Every other platform is unaffected, with or without the option
"$cc" -E -DESP32=0 "$work/guard.c" >/dev/null 2>"$work/err.log" ||
    fail "a non-ESP32 build was refused: $(head -3 "$work/err.log")"
checked=$((checked + 1))
"$cc" -E "$work/guard.c" >/dev/null 2>"$work/err.log" ||
    fail "a build that does not define ESP32 was refused: $(head -3 "$work/err.log")"
checked=$((checked + 1))

rm -rf "$work"
echo "✓ ESP32 MbedTLS clock requirement enforced ($checked checks)"
exit 0
