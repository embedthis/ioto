#!/usr/bin/env bash
#   Agent web.json5 files use bare keys because the agent blends them under web. Standalone webAllocHost
#   configurations require the web wrapper. Check apps, samples and tests for the appropriate shape.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT" || exit 1
status=0

#   A top-level "web" key: the first non-comment, non-blank line after the opening brace.
wrapped() {
    grep -qE '^[[:space:]]*(web|"web"|'"'"'web'"'"')[[:space:]]*:[[:space:]]*\{' "$1"
}

#
#   Agent applications and agent-linked samples: bare. Samples with their own main(): wrapped.
#
for file in apps/*/web.json5 samples/*/web.json5 ; do
    [ -f "$file" ] || continue
    #   A sample with its own main() that hands the file to webAllocHost is a bin/web consumer
    src="$(dirname "$file")/sample.c"
    if [ -f "$src" ] && grep -qE '^int main\(' "$src" && grep -q 'webAllocHost(' "$src" ; then
        if wrapped "$file" ; then
            echo "✓ $file keeps the wrapper its own webAllocHost call requires"
        else
            echo "✗ $file is bare but $src passes it to webAllocHost, which reads web.listen"
            status=1
        fi
        continue
    fi
    if wrapped "$file" ; then
        echo "✗ $file wraps its settings in 'web: { }', which the agent loader blends to web.web.*"
        status=1
    else
        echo "✓ $file is in the agent's bare shape"
    fi
done

#
#   The standalone server's own configuration: wrapped, and a bare one would leave it with no
#   listeners at all. Asserted so this guard cannot be "fixed" by flattening the wrong files.
#
for file in test/web/web.json5 test/url/web.json5 ; do
    [ -f "$file" ] || continue
    if wrapped "$file" ; then
        echo "✓ $file keeps the wrapper bin/web requires"
    else
        echo "✗ $file lost its 'web: { }' wrapper, which bin/web requires"
        status=1
    fi
done

if [ "$status" -eq 0 ] ; then
    echo "✓ config-shape: every web.json5 matches the shape its consumer reads"
else
    echo "✗ config-shape: a web.json5 is in the shape its consumer does not read" >&2
fi
exit $status
