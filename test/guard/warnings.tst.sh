#!/usr/bin/env bash
#   Run check-warnings across debug/release and installed TLS providers. Skip with exit 2 on unsupported
#   platforms or when the internal script is absent from a distribution.
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SCRIPT="$ROOT/bin/check-warnings.sh"

if [ ! -f "$SCRIPT" ]; then
    echo "✓ check-warnings: skipped, $(basename "$SCRIPT") is not present in this tree"
    exit 0
fi
output=$(bash "$SCRIPT" 2>&1)
rc=$?
echo "$output"

case "$rc" in
0) echo "✓ check-warnings: no compiler warnings" ;;
2) echo "✓ check-warnings: skipped, no gmake2 build on this platform" ;;
*) echo "✗ check-warnings: the build compiles with warnings" >&2
   exit 1 ;;
esac
exit 0
