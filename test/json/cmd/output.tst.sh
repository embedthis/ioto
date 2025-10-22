#!/bin/bash
#
# output.tst.sh - Tests for --out and --overwrite options
#

PID=$$
FAIL=0

pass() { echo "  ✓ $1"; }
fail() { echo "  ✗ $1"; FAIL=$((FAIL + 1)); }
contains() { echo "$1" | grep -q "$2"; }

# Test --out option writes to specified file
echo "Testing --out option..."
TMPIN="/tmp/json-test-in-$PID.json"
TMPOUT="/tmp/json-test-out-$PID.json"
echo "{name: 'Alice', age: 30}" > "$TMPIN"
json --out "$TMPOUT" "$TMPIN" >/dev/null 2>&1
if [ $? -eq 0 ] && [ -f "$TMPOUT" ] && contains "$(cat $TMPOUT)" "Alice"; then
    pass "json --out writes to output file"
else
    fail "json --out failed to write output file"
fi
rm -f "$TMPIN" "$TMPOUT"

# Test --out with format conversion
echo "Testing --out with format conversion..."
TMPIN="/tmp/json-test-in-$PID.json5"
TMPOUT="/tmp/json-test-out-$PID.json"
echo "{name: 'Bob', age: 25}" > "$TMPIN"
json --json --out "$TMPOUT" "$TMPIN" >/dev/null 2>&1
if [ $? -eq 0 ] && contains "$(cat $TMPOUT)" '"name"'; then
    pass "json --out with --json converts format"
else
    fail "json --out with --json failed"
fi
rm -f "$TMPIN" "$TMPOUT"

# Test that --out does not modify input file
echo "Testing --out preserves input file..."
TMPIN="/tmp/json-test-in-$PID.json"
TMPOUT="/tmp/json-test-out-$PID.json"
ORIGINAL="{name: 'Charlie'}"
echo "$ORIGINAL" > "$TMPIN"
json --out "$TMPOUT" "$TMPIN" >/dev/null 2>&1
AFTER=$(cat "$TMPIN")
if [ "$ORIGINAL" = "$AFTER" ]; then
    pass "json --out preserves input file"
else
    fail "json --out modified input file"
fi
rm -f "$TMPIN" "$TMPOUT"

# Test --out and --overwrite are mutually exclusive
echo "Testing --out and --overwrite mutual exclusivity..."
TMPIN="/tmp/json-test-$PID.json"
TMPOUT="/tmp/json-test-out-$PID.json"
echo "{test: 'data'}" > "$TMPIN"
OUTPUT=$(json --out "$TMPOUT" --overwrite "$TMPIN" 2>&1)
if [ $? -ne 0 ] && (contains "$OUTPUT" "both" || contains "$OUTPUT" "out" || contains "$OUTPUT" "overwrite"); then
    pass "json --out and --overwrite are mutually exclusive"
else
    fail "json should reject --out and --overwrite together"
fi
rm -f "$TMPIN" "$TMPOUT"

# Test --overwrite modifies file in place
echo "Testing --overwrite..."
TMPFILE="/tmp/json-test-$PID.json"
echo "{name: 'David', age: 35}" > "$TMPFILE"
json --overwrite --json "$TMPFILE" >/dev/null 2>&1
if [ $? -eq 0 ] && contains "$(cat $TMPFILE)" '"name"'; then
    pass "json --overwrite modifies file in place"
else
    fail "json --overwrite failed"
fi
rm -f "$TMPFILE"

# Test --out with assignment
echo "Testing --out with assignment..."
TMPIN="/tmp/json-test-in-$PID.json"
TMPOUT="/tmp/json-test-out-$PID.json"
echo "{user: {name: 'Eve'}}" > "$TMPIN"
json --out "$TMPOUT" user.email=eve@example.com "$TMPIN" >/dev/null 2>&1
if [ $? -eq 0 ] && contains "$(cat $TMPOUT)" "email" && contains "$(cat $TMPOUT)" "eve@example.com"; then
    pass "json --out with assignment works"
else
    fail "json --out with assignment failed"
fi
# Verify input not modified
if ! contains "$(cat $TMPIN)" "email"; then
    pass "json --out with assignment preserves input"
else
    fail "json --out with assignment modified input"
fi
rm -f "$TMPIN" "$TMPOUT"

# Summary
echo ""
if [ $FAIL -eq 0 ]; then
    echo "All tests passed"
    exit 0
else
    echo "$FAIL test(s) failed"
    exit 1
fi
