#!/usr/bin/env bash
#
#   apps-csp-inline.tst.sh - Shipped app pages must run under the Content-Security-Policy the app ships
#
#   apps/http/web.json5 sends "Content-Security-Policy: default-src 'self'", which forbids inline script
#   and data: URIs. Every page of the apps/http site carried its login-state logic in an inline <script>,
#   so a browser refused to run it and the shipped UI never showed the login state. An integrator fixing
#   that by adding 'unsafe-inline' would also re-enable the innerHTML sinks those scripts wrote server
#   data into.
#
#   Fails when an app whose web.json5 CSP lacks 'unsafe-inline' ships an inline <script>, an inline
#   event handler attribute or a data: URI in its site, or assigns with innerHTML. The root is derived
#   from this script's own location, and a run that checks no page fails, so the guard cannot pass by
#   looking in the wrong place.
#
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
FAILED=0
PAGES=0

for config in "$ROOT"/apps/*/web.json5; do
    app=$(dirname "$config")
    site="$app/site"
    [ -d "$site" ] || continue
    csp=$(grep -i "Content-Security-Policy" "$config")
    [ -n "$csp" ] || continue
    if echo "$csp" | grep -q "unsafe-inline"; then
        continue
    fi
    while IFS= read -r page; do
        PAGES=$((PAGES + 1))
        if grep -E '<script( [^>]*)?>' "$page" | grep -vq 'src='; then
            echo "✗ inline <script> blocked by the app CSP: ${page#$ROOT/}"
            FAILED=1
        fi
        if grep -Eiq '<[^>]+ on[a-z]+=' "$page"; then
            echo "✗ inline event handler blocked by the app CSP: ${page#$ROOT/}"
            FAILED=1
        fi
        if grep -Eq '(src|href)="data:' "$page"; then
            echo "✗ data: URI blocked by the app CSP: ${page#$ROOT/}"
            FAILED=1
        fi
    done < <(find "$site" -name '*.html' -type f)
    while IFS= read -r script; do
        if grep -q 'innerHTML' "$script"; then
            echo "✗ innerHTML assignment in shipped page or script: ${script#$ROOT/}"
            FAILED=1
        fi
    done < <(find "$site" \( -name '*.js' -o -name '*.html' \) -type f)
done

if [ $PAGES -eq 0 ]; then
    echo "✗ no app page was checked under $ROOT/apps; the guard is looking in the wrong place"
    exit 1
fi
if [ $FAILED -ne 0 ]; then
    exit 1
fi
echo "✓ $PAGES shipped app pages are compatible with their Content-Security-Policy"
exit 0
