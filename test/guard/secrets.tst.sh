#!/usr/bin/env bash
#
#   secrets.tst.sh - Fail the suite when private key material is committed outside certs/
#
#   test/mqtt carried an Amazon-issued IoT device certificate and its RSA private key, and shipped
#   both in every source archive. Nothing failed when they were committed: no test used them, and
#   bin/make-dist stages test/ by directory, so the files were never named anywhere a reviewer
#   would look. A credential is not caught by the tests it is absent from - it needs a check that
#   asks the question directly.
#
#   This is the commit side. The ship side is a preflight inside bin/make-dist itself, which
#   refuses to pack a staging tree containing key material; that check has to live in the producer
#   rather than here, because a test nobody remembered to run is what this ticket is about.
#
#   Exit 2 from the script means there is nothing to scan - test/ ships in the source distribution,
#   where there is no git index and no bin/check-secrets.sh. That is a skip, and it is announced
#   rather than swallowed, so "nothing to check" is never read as "everything was clean".
#
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SCRIPT="$ROOT/bin/check-secrets.sh"

if [ ! -f "$SCRIPT" ]; then
    echo "✓ secrets: skipped, check-secrets.sh is not present in this tree"
    exit 0
fi

output=$(bash "$SCRIPT" --index 2>&1)
rc=$?
echo "$output"

case "$rc" in
0) echo "✓ secrets: no private key material outside certs/" ; exit 0 ;;
2) echo "✓ secrets: skipped, there is no git index to scan in this tree" ; exit 0 ;;
*) echo "✗ secrets: private key material is committed outside certs/" >&2 ; exit 1 ;;
esac
