#!/usr/bin/env bash
#   All shipped ESP32 defaults must enable the runtime-required TLS clock setting and the strong stack protector.
#   Checks files without ESP-IDF.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
REQUIRED="CONFIG_MBEDTLS_HAVE_TIME_DATE=y CONFIG_COMPILER_STACK_CHECK_MODE_STRONG=y"
status=0
count=0

for f in "$ROOT"/apps/*/esp32/sdkconfig.defaults ; do
    [ -f "$f" ] || continue
    count=$((count + 1))
    for key in $REQUIRED ; do
        if ! grep -qx "$key" "$f" ; then
            echo "✗ ${f#$ROOT/} does not set $key" >&2
            status=1
        fi
    done
done

if [ $count -eq 0 ] ; then
    echo "✗ no apps/*/esp32/sdkconfig.defaults found" >&2
    exit 1
fi
if [ $status -eq 0 ] ; then
    echo "✓ $count ESP32 sdkconfig.defaults set the required options"
fi
exit $status
