#!/usr/bin/env bash
#
#   shipped-trust.tst.sh - No CA bundle shipped under certs/ or samples/ may hold an expired root
#
#   A sample that ships its own stale bundle teaches integrators to trust it, and a shipped
#   certs/roots*.crt with expired roots means the product's default trust store is out of date.
#
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
status=0

if ! command -v openssl >/dev/null 2>&1 ; then
    echo "✓ shipped-trust: skipped, openssl is not installed"
    exit 0
fi

bundles=$(cd "$ROOT" && { ls certs/roots*.crt 2>/dev/null; find samples -type f 2>/dev/null; } |
    while read -r f ; do grep -l 'BEGIN CERTIFICATE' "$ROOT/$f" 2>/dev/null ; done)

for bundle in $bundles ; do
    total=0
    expired=0
    while read -r pem ; do
        total=$((total + 1))
        if ! echo "$pem" | tr '|' '\n' | openssl x509 -noout -checkend 0 >/dev/null 2>&1 ; then
            expired=$((expired + 1))
        fi
    done < <(awk '/BEGIN CERT/ { c = "" } { c = c $0 "|" } /END CERT/ { print c }' "$bundle")
    name="${bundle#$ROOT/}"
    if [ "$expired" -gt 0 ] ; then
        echo "✗ $name has $expired expired of $total certificates"
        status=1
    else
        echo "✓ $name: $total certificates, none expired"
    fi
done
exit $status
