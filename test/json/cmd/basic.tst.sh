#!/bin/bash
#
# basic.tst.sh - Basic command-line tests for the json utility
#

PID=$$
FAIL=0

# Test helper functions
pass() {
    echo "  ✓ $1"
}

fail() {
    echo "  ✗ $1"
    FAIL=$((FAIL + 1))
}

contains() {
    if echo "$1" | grep -q "$2"; then
        return 0
    else
        return 1
    fi
}

# Test --version flag
echo "Testing --version..."
OUTPUT=$(json --version 2>&1)
if [ $? -eq 0 ] && [ -n "$OUTPUT" ]; then
    pass "json --version returns version"
else
    fail "json --version failed"
fi

# Test --check with valid JSON
echo "Testing --check with valid JSON..."
TMPFILE="/tmp/json-test-$PID.json"
echo "{name: 'Alice', age: 30}" > "$TMPFILE"
json --check "$TMPFILE" >/dev/null 2>&1
if [ $? -eq 0 ]; then
    pass "json --check accepts valid JSON"
else
    fail "json --check rejected valid JSON"
fi

# Test --check with invalid JSON
echo "Testing --check with invalid JSON..."
echo "{name: 'Alice', age:" > "$TMPFILE"
json --check "$TMPFILE" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    pass "json --check rejects invalid JSON"
else
    fail "json --check should reject invalid JSON"
fi
rm -f "$TMPFILE"

# Test reading from stdin
echo "Testing stdin input..."
OUTPUT=$(echo "{name: 'Bob', role: 'developer'}" | json --stdin 2>&1)
if [ $? -eq 0 ] && contains "$OUTPUT" "name" && contains "$OUTPUT" "Bob"; then
    pass "json --stdin reads from stdin"
else
    fail "json --stdin failed to read from stdin"
fi

# Test empty input
echo "Testing empty input..."
OUTPUT=$(echo "" | json --stdin 2>&1)
if [ $? -eq 0 ]; then
    pass "json --stdin handles empty input"
else
    fail "json --stdin failed on empty input"
fi

# Test piped input (stdin is default when no file given)
echo "Testing piped input..."
OUTPUT=$(echo "{name: 'Charlie'}" | json 2>&1)
if [ $? -eq 0 ] && contains "$OUTPUT" "Charlie"; then
    pass "json accepts piped input"
else
    fail "json failed to accept piped input"
fi

# Summary
echo ""
if [ $FAIL -eq 0 ]; then
    echo "All tests passed"
    exit 0
else
    echo "$FAIL test(s) failed"
    exit 1
fi
