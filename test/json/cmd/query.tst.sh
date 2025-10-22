#!/bin/bash
#
# query.tst.sh - Query and modify operation tests for the json utility
#

PID=$$
FAIL=0

pass() { echo "  ✓ $1"; }
fail() { echo "  ✗ $1"; FAIL=$((FAIL + 1)); }
contains() { echo "$1" | grep -q "$2"; }

# Test simple property query
echo "Testing simple query..."
TMPFILE="/tmp/json-test-$PID.json"
echo "{name: 'Alice', age: 30, city: 'NYC'}" > "$TMPFILE"
OUTPUT=$(json name "$TMPFILE" 2>&1)
if [ $? -eq 0 ] && contains "$OUTPUT" "Alice"; then
    pass "json queries simple properties"
else
    fail "json simple query failed"
fi
rm -f "$TMPFILE"

# Test nested query with dot notation
echo "Testing nested query..."
TMPFILE="/tmp/json-test-$PID.json"
echo "{user: {profile: {name: 'Bob', email: 'bob@example.com'}}}" > "$TMPFILE"
OUTPUT=$(json user.profile.email "$TMPFILE" 2>&1)
if [ $? -eq 0 ] && contains "$OUTPUT" "bob@example.com"; then
    pass "json queries nested properties with dot notation"
else
    fail "json nested query failed"
fi
rm -f "$TMPFILE"

# Test array query
echo "Testing array query..."
TMPFILE="/tmp/json-test-$PID.json"
echo "{users: [{name: 'Alice'}, {name: 'Bob'}, {name: 'Charlie'}]}" > "$TMPFILE"
OUTPUT=$(json users.1.name "$TMPFILE" 2>&1)
if [ $? -eq 0 ] && contains "$OUTPUT" "Bob"; then
    pass "json queries array elements by index"
else
    fail "json array query failed"
fi
rm -f "$TMPFILE"

# Test query with default value
echo "Testing --default..."
TMPFILE="/tmp/json-test-$PID.json"
echo "{name: 'Dana'}" > "$TMPFILE"
OUTPUT=$(json --default unknown email "$TMPFILE" 2>&1)
if [ $? -eq 0 ] && contains "$OUTPUT" "unknown"; then
    pass "json --default provides fallback value"
else
    fail "json --default failed"
fi
rm -f "$TMPFILE"

# Test setting a property
echo "Testing property assignment..."
TMPFILE="/tmp/json-test-$PID.json"
echo "{name: 'Eve', age: 25}" > "$TMPFILE"
OUTPUT=$(json age=26 "$TMPFILE" 2>&1)
if [ $? -eq 0 ] && contains "$OUTPUT" "26" && contains "$OUTPUT" "Eve"; then
    pass "json sets property values"
else
    fail "json property assignment failed"
fi
rm -f "$TMPFILE"

# Test setting nested property
echo "Testing nested property assignment..."
TMPFILE="/tmp/json-test-$PID.json"
echo "{user: {name: 'Frank'}}" > "$TMPFILE"
OUTPUT=$(json user.email=frank@example.com "$TMPFILE" 2>&1)
if [ $? -eq 0 ] && contains "$OUTPUT" "email" && contains "$OUTPUT" "frank@example.com"; then
    pass "json sets nested properties"
else
    fail "json nested property assignment failed"
fi
rm -f "$TMPFILE"

# Test removing a property
echo "Testing --remove..."
TMPFILE="/tmp/json-test-$PID.json"
echo "{name: 'Grace', age: 35, city: 'LA'}" > "$TMPFILE"
OUTPUT=$(json --remove city "$TMPFILE" 2>&1)
if [ $? -eq 0 ] && contains "$OUTPUT" "Grace" && ! contains "$OUTPUT" "city" && ! contains "$OUTPUT" "LA"; then
    pass "json --remove deletes properties"
else
    fail "json --remove failed"
fi
rm -f "$TMPFILE"

# Test version bumping
echo "Testing --bump..."
TMPFILE="/tmp/json-test-$PID.json"
echo "{name: 'myapp', version: '1.2.3'}" > "$TMPFILE"
OUTPUT=$(json --bump version "$TMPFILE" 2>&1)
if [ $? -eq 0 ] && contains "$OUTPUT" "1.2.4" && ! contains "$OUTPUT" "1.2.3"; then
    pass "json --bump increments version numbers"
else
    fail "json --bump failed"
fi
rm -f "$TMPFILE"

# Test bumping numeric version
echo "Testing --bump with number..."
TMPFILE="/tmp/json-test-$PID.json"
echo "{build: 42}" > "$TMPFILE"
OUTPUT=$(json --bump build "$TMPFILE" 2>&1)
if [ $? -eq 0 ] && contains "$OUTPUT" "43"; then
    pass "json --bump increments numeric values"
else
    fail "json --bump numeric failed"
fi
rm -f "$TMPFILE"

# Test --keys
echo "Testing --keys..."
TMPFILE="/tmp/json-test-$PID.json"
echo "{name: 'Henry', age: 40, role: 'manager'}" > "$TMPFILE"
OUTPUT=$(json --keys . "$TMPFILE" 2>&1)
if [ $? -eq 0 ] && contains "$OUTPUT" "name" && contains "$OUTPUT" "age" && contains "$OUTPUT" "role"; then
    pass "json --keys lists property names"
else
    fail "json --keys failed"
fi
rm -f "$TMPFILE"

# Test --env format
echo "Testing --env..."
TMPFILE="/tmp/json-test-$PID.json"
echo "{dbHost: 'localhost', dbPort: 5432}" > "$TMPFILE"
OUTPUT=$(json --env . "$TMPFILE" 2>&1)
if [ $? -eq 0 ] && contains "$OUTPUT" "DB_HOST" && contains "$OUTPUT" "localhost"; then
    pass "json --env outputs environment variable format"
else
    fail "json --env failed"
fi
rm -f "$TMPFILE"

# Test --header format
echo "Testing --header..."
TMPFILE="/tmp/json-test-$PID.json"
echo "{maxSize: 1024, enabled: true}" > "$TMPFILE"
OUTPUT=$(json --header . "$TMPFILE" 2>&1)
if [ $? -eq 0 ] && contains "$OUTPUT" "#define" && contains "$OUTPUT" "MAX_SIZE"; then
    pass "json --header outputs C header format"
else
    fail "json --header failed"
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
