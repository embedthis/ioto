#!/usr/bin/env bash
#
#   healthcheck-probe.tst.sh - Each group's healthcheck probes the document its setup script identifies
#
#   A setup script that names a PROBE document starts or adopts the server by that document. The
#   group's TestMe healthcheck must probe the same document, or it accepts a server the setup script
#   would refuse: every checkout serves an index.html, but only this checkout's server serves its
#   own suite identity.
#
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
status=0
count=0

for config in "$ROOT"/test/*/testme.json5 "$ROOT"/test/*/*/testme.json5 ; do
    [ -f "$config" ] || continue
    dir=$(dirname "$config")
    setup=$(sed -n "s/^[[:space:]]*setup:[[:space:]]*'\([^']*\)'.*/\1/p" "$config")
    url=$(sed -n "s/.*healthcheck:.*url:[[:space:]]*'\([^']*\)'.*/\1/p" "$config")
    [ -n "$setup" ] && [ -n "$url" ] && [ -f "$dir/$setup" ] || continue
    #   The PROBE may live in a helper the setup script sources
    sources="$dir/$setup $(sed -n "s|^\. \./\([^ ]*\).*|$dir/\1|p" "$dir/$setup")"
    probe=$(cat $sources 2>/dev/null | sed -n "s/^PROBE=\(.*\)/\1/p" | head -1)
    [ -n "$probe" ] || continue
    count=$((count + 1))
    path=${url#*://*/}
    path=${path%%\?*}
    if [ "$path" != "$probe" ] ; then
        echo "✗ ${config#$ROOT/} healthcheck probes $path, but $setup identifies its server by $probe" >&2
        status=1
    fi
done

if [ $count -eq 0 ] ; then
    echo "✗ no group pairs a healthcheck with a setup script that names a PROBE" >&2
    exit 1
fi
if [ $status -eq 0 ] ; then
    echo "✓ $count group healthchecks probe the document their setup script identifies"
fi
exit $status
