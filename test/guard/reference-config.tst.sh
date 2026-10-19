#!/usr/bin/env bash
#   The reference and sample web configurations are copied as templates, so they must model only settings
#   that are safe to copy: finite timeouts, no hand-written CORS header, no header dumping, loopback
#   listeners, a private upload directory, and HSTS only where every plaintext listener redirects to https.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT" || exit 1
status=0

#   Configuration text with comments removed: /* */ blocks and whole-line // comments. A // inside a
#   value, as in 'http://host', is kept.
code() {
    awk '
        {
            line = $0
            out = ""
            while (length(line) > 0) {
                if (inBlock) {
                    end = index(line, "*/")
                    if (end == 0) { line = ""; break }
                    line = substr(line, end + 2)
                    inBlock = 0
                } else {
                    start = index(line, "/*")
                    if (start == 0) { out = out line; line = ""; break }
                    out = out substr(line, 1, start - 1)
                    line = substr(line, start + 2)
                    inBlock = 1
                }
            }
            if (out !~ /^[[:space:]]*\/\//) print out
        }' "$1"
}

check() {   # file, extended regex, message
    [ -f "$1" ] || return 0
    if code "$1" | grep -qE "$2" ; then
        echo "✗ $1: $3"
        status=1
    fi
}

#   The endpoints of the listen array, which may span lines
listeners() {
    code "$1" | tr '\n' ' ' | grep -oE 'listen:[[:space:]]*\[[^]]*\]' | grep -oE "[a-z]+://[^'\"]+"
}

#   Every http:// listener of a config that sends Strict-Transport-Security must be a redirect source
hsts() {
    local file="$1" text listener
    text="$(code "$file")"
    echo "$text" | grep -q 'Strict-Transport-Security' || return 0
    for listener in $(listeners "$file" | grep '^http://') ; do
        if ! echo "$text" | grep -E 'from:' | grep -qF "'${listener}'" && \
           ! echo "$text" | grep -E 'from:' | grep -qF "\"${listener}\"" ; then
            echo "✗ $file: sends Strict-Transport-Security but $listener serves plaintext without a redirect"
            status=1
        fi
    done
}

CONFIGS="doc/web.json5 $(ls apps/*/web.json5 samples/*/web.json5 2>/dev/null)"

check doc/web.json5 "request:[[:space:]]*'infinite'" "request timeout is infinite"
check doc/web.json5 "CrossOrigin" "models a CrossOrigin header that no code reads (origin=* credentials=yes)"
for f in $CONFIGS ; do
    [ -f "$f" ] || continue
    check "$f" "show:[[:space:]]*'[^']*[hH]" "enables header dumping, which logs Authorization and Cookie"
    if listeners "$f" | grep -qE '^https?://(:|0\.0\.0\.0|\[::\])' ; then
        echo "✗ $f: listens on all interfaces"
        status=1
    fi
    check "$f" "dir:[[:space:]]*['\"]/tmp" "upload directory is the shared /tmp"
    hsts "$f"
done
[ -f apps/http/site/index.html ] && \
    check apps/http/site/index.html "password of \"demo\"" "advertises the removed demo accounts"

if [ "$status" -eq 0 ] ; then
    echo "✓ reference-config: the reference and sample configurations model only safe settings"
fi
exit $status
