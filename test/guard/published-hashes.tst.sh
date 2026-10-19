#!/usr/bin/env bash
#
#   published-hashes.tst.sh - The published "demo" password hashes must not ship anywhere in the tree
#
#   The 3.1 http template seeded admin, guest and super with the password "demo", and their hashes were
#   published in the source distribution. Test fixtures kept carrying them after the template stopped,
#   and test/ ships, so the archive still held a known administrative credential. Each hash is matched by
#   its salt, which is unique to it.
#
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SALTS='4oMqal5QuyBIslkR|9bK672CWJNqRGK7T|BYVWyfEQCG0Ypdwf'

if [ ! -d "$ROOT/apps" ] || [ ! -d "$ROOT/test" ]; then
    echo "✗ $ROOT has no apps/ or test/; the guard is looking in the wrong place"
    exit 1
fi
#   In a checkout, scan what is committed: generated test state is rebuilt from the fixtures on each run.
#   In a source archive, there is no index, so scan the files. Skip devices: a test run leaves a FIFO in
#   test/web/site, and reading it blocks forever.
if git -C "$ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    found=$(cd "$ROOT" && git ls-files -z apps test samples | xargs -0 grep -lE "$SALTS" 2>/dev/null)
else
    found=$(cd "$ROOT" && grep -rlE -D skip "$SALTS" apps test samples 2>/dev/null)
fi
found=$(echo "$found" | grep -v "^test/guard/published-hashes.tst.sh$" | grep -v '^$')
if [ -n "$found" ]; then
    echo "✗ published demo password hashes found in:"
    echo "$found" | sed "s|^|    |"
    exit 1
fi
echo "✓ no published demo password hash in apps/, test/ or samples/"
exit 0
