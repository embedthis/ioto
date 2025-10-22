#!/bin/bash
#
# convert.tst.sh - Format conversion tests for the json utility
#

PID=$$
FAIL=0

pass() { echo "  ✓ $1"; }
fail() { echo "  ✗ $1"; FAIL=$((FAIL + 1)); }
contains() { echo "$1" | grep -q "$2"; }

# Test JSON5 to JSON conversion
echo "Testing JSON5 to JSON conversion..."
TMPFILE="/tmp/json-test-$PID.json5"
echo "{name: 'Alice', age: 30, active: true,}" > "$TMPFILE"
OUTPUT=$(json --json "$TMPFILE" 2>&1)
if [ $? -eq 0 ] && contains "$OUTPUT" '"name"' && contains "$OUTPUT" '"Alice"'; then
    pass "json --json converts JSON5 to strict JSON"
else
    fail "json --json conversion failed"
fi
rm -f "$TMPFILE"

# Test JSON to JSON5 conversion
echo "Testing JSON to JSON5 conversion..."
TMPFILE="/tmp/json-test-$PID.json"
echo '{"name": "Bob", "age": 25}' > "$TMPFILE"
OUTPUT=$(json --json5 "$TMPFILE" 2>&1)
if [ $? -eq 0 ] && contains "$OUTPUT" "Bob"; then
    pass "json --json5 converts to JSON5 format"
else
    fail "json --json5 conversion failed"
fi
rm -f "$TMPFILE"

# Test compact output
echo "Testing --compact..."
TMPFILE="/tmp/json-test-$PID.json"
echo '{
  "name": "Charlie",
  "age": 40,
  "city": "NYC"
}' > "$TMPFILE"
OUTPUT=$(json --compact "$TMPFILE" 2>&1)
# Just check it succeeds - compact output can vary
if [ $? -eq 0 ]; then
    pass "json --compact produces output"
else
    fail "json --compact failed"
fi
rm -f "$TMPFILE"

# Test JS format
echo "Testing --js format..."
TMPFILE="/tmp/json-test-$PID.json"
echo "{name: 'Dana', role: 'engineer'}" > "$TMPFILE"
OUTPUT=$(json --js "$TMPFILE" 2>&1)
if [ $? -eq 0 ] && contains "$OUTPUT" "export"; then
    pass "json --js produces JavaScript module format"
else
    fail "json --js failed"
fi
rm -f "$TMPFILE"

# Test one-line output
echo "Testing --one..."
TMPFILE="/tmp/json-test-$PID.json"
echo '{
  "name": "Eve",
  "items": [1, 2, 3]
}' > "$TMPFILE"
OUTPUT=$(json --one "$TMPFILE" 2>&1)
LINES=$(echo "$OUTPUT" | wc -l | tr -d ' ')
if [ $? -eq 0 ] && [ "$LINES" -le 1 ]; then
    pass "json --one produces single-line output"
else
    fail "json --one failed (lines: $LINES)"
fi
rm -f "$TMPFILE"

# Test double quotes
echo "Testing --double..."
TMPFILE="/tmp/json-test-$PID.json"
echo "{name: 'Frank'}" > "$TMPFILE"
OUTPUT=$(json --double "$TMPFILE" 2>&1)
if [ $? -eq 0 ] && contains "$OUTPUT" '"Frank"' && ! contains "$OUTPUT" "'Frank'"; then
    pass "json --double uses double quotes"
else
    fail "json --double failed"
fi
rm -f "$TMPFILE"

# Test single quotes
echo "Testing --single..."
TMPFILE="/tmp/json-test-$PID.json"
echo '{"name": "Grace"}' > "$TMPFILE"
OUTPUT=$(json --single "$TMPFILE" 2>&1)
if [ $? -eq 0 ] && contains "$OUTPUT" "'Grace'"; then
    pass "json --single uses single quotes"
else
    fail "json --single failed"
fi
rm -f "$TMPFILE"

# Test strict parsing (should reject trailing comma)
echo "Testing --strict parsing..."
TMPFILE="/tmp/json-test-$PID.json"
echo '{"name": "Henry", "age": 50,}' > "$TMPFILE"
json --strict "$TMPFILE" >/dev/null 2>&1
STRICT_RESULT=$?
json "$TMPFILE" >/dev/null 2>&1
RELAXED_RESULT=$?
if [ $STRICT_RESULT -ne 0 ] && [ $RELAXED_RESULT -eq 0 ]; then
    pass "json --strict rejects non-standard JSON"
else
    fail "json --strict should reject trailing commas"
fi
rm -f "$TMPFILE"

# Summary
echo ""
if [ $FAIL -eq 0 ]; then
    echo "All tests passed"
    exit 0
else
    echo "$FAIL test(s) failed"
    exit 1
fi
