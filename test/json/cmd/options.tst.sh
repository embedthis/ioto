#!/bin/bash
#
# options.tst.sh - Option, command and error-path tests for the json utility
#

PID=$$
FAIL=0

pass() { echo "  ✓ $1"; }
fail() { echo "  ✗ $1"; FAIL=$((FAIL + 1)); }
contains() { echo "$1" | grep -q "$2"; }

DIR="/tmp/json-test-opt-$PID"
mkdir -p "$DIR"

#
#   Option parsing errors. Each must fail and print the usage banner.
#
echo "Testing option parsing errors..."
for OPT in --unknown -x; do
    OUTPUT=$(json $OPT 2>&1)
    if [ $? -ne 0 ] && contains "$OUTPUT" "usage: json"; then
        pass "json $OPT reports usage"
    else
        fail "json $OPT did not report usage"
    fi
done

echo "Testing options with a missing argument..."
for OPT in --bump --default --indent --length --out --profile --trace; do
    OUTPUT=$(json $OPT 2>&1)
    if [ $? -ne 0 ] && contains "$OUTPUT" "usage: json"; then
        pass "json $OPT with no argument reports usage"
    else
        fail "json $OPT with no argument did not report usage"
    fi
done

echo "Testing too many arguments..."
echo "{a: 1}" > "$DIR/in.json5"
OUTPUT=$(json a b "$DIR/in.json5" 2>&1)
if [ $? -ne 0 ] && contains "$OUTPUT" "usage: json"; then
    pass "json rejects surplus arguments"
else
    fail "json accepted surplus arguments"
fi

echo "Testing --out with --overwrite..."
OUTPUT=$(json --out "$DIR/out.json" --overwrite "$DIR/in.json5" 2>&1)
if [ $? -ne 0 ] && contains "$OUTPUT" "usage: json"; then
    pass "json rejects --out with --overwrite"
else
    fail "json accepted --out with --overwrite"
fi

echo "Testing a missing input file..."
OUTPUT=$(json a "$DIR/no-such-file.json" 2>&1)
if [ $? -ne 0 ]; then
    pass "json fails on a missing input file"
else
    fail "json did not fail on a missing input file"
fi

OUTPUT=$(json --noerror a "$DIR/no-such-file.json" 2>&1)
if [ $? -ne 0 ] || [ -z "$OUTPUT" ]; then
    pass "json --noerror tolerates a missing input file"
else
    fail "json --noerror did not tolerate a missing input file"
fi

echo "Testing unparsable input..."
echo "{name:" > "$DIR/bad.json5"
OUTPUT=$(json a "$DIR/bad.json5" 2>&1)
if [ $? -ne 0 ] && contains "$OUTPUT" "Cannot parse"; then
    pass "json reports an unparsable file"
else
    fail "json did not report an unparsable file"
fi

OUTPUT=$(json --quiet a "$DIR/bad.json5" 2>&1)
if [ $? -ne 0 ] && [ -z "$OUTPUT" ]; then
    pass "json --quiet suppresses the error message"
else
    fail "json --quiet still emitted output"
fi

#
#   --check
#
echo "Testing --check..."
if json --check "$DIR/in.json5" >/dev/null 2>&1; then
    pass "json --check accepts valid input"
else
    fail "json --check rejected valid input"
fi
if json --check "$DIR/bad.json5" >/dev/null 2>&1; then
    fail "json --check accepted invalid input"
else
    pass "json --check rejects invalid input"
fi

#
#   --bump
#
echo "Testing --bump..."
echo "{version: '1.2.3'}" > "$DIR/version.json5"
OUTPUT=$(json --bump version "$DIR/version.json5" 2>&1)
if [ $? -eq 0 ] && contains "$OUTPUT" "1.2.4"; then
    pass "json --bump increments a dotted version"
else
    fail "json --bump did not increment a dotted version"
fi

echo "{build: 7}" > "$DIR/build.json5"
OUTPUT=$(json --bump build "$DIR/build.json5" 2>&1)
if [ $? -eq 0 ] && contains "$OUTPUT" "8"; then
    pass "json --bump increments a numeric property"
else
    fail "json --bump did not increment a numeric property"
fi

OUTPUT=$(json --bump missing "$DIR/version.json5" 2>&1)
if [ $? -ne 0 ]; then
    pass "json --bump fails on a missing property"
else
    fail "json --bump did not fail on a missing property"
fi

echo "Testing --bump --overwrite..."
json --bump version --overwrite "$DIR/version.json5" >/dev/null 2>&1
OUTPUT=$(json version "$DIR/version.json5" 2>&1)
if [ "$OUTPUT" = "1.2.4" ]; then
    pass "json --bump --overwrite updates the file in place"
else
    fail "json --bump --overwrite left the file at '$OUTPUT'"
fi

#
#   --overwrite and --out
#
echo "Testing --overwrite..."
echo "{name: 'Alice'}" > "$DIR/assign.json5"
json --overwrite name=Bob "$DIR/assign.json5" >/dev/null 2>&1
OUTPUT=$(json name "$DIR/assign.json5" 2>&1)
if [ "$OUTPUT" = "Bob" ]; then
    pass "json property=value --overwrite updates the file in place"
else
    fail "json --overwrite assignment left the file at '$OUTPUT'"
fi

json --remove --overwrite name "$DIR/assign.json5" >/dev/null 2>&1
OUTPUT=$(json --default absent name "$DIR/assign.json5" 2>&1)
if [ "$OUTPUT" = "absent" ]; then
    pass "json --remove --overwrite removes the property in place"
else
    fail "json --remove --overwrite left '$OUTPUT'"
fi

echo "Testing --out..."
json --out "$DIR/out.json" --json "$DIR/in.json5" >/dev/null 2>&1
if [ -f "$DIR/out.json" ] && contains "$(cat "$DIR/out.json")" '"a"'; then
    pass "json --out writes to the named file"
else
    fail "json --out did not write the named file"
fi

echo "Testing --overwrite conversion..."
cp "$DIR/in.json5" "$DIR/convert.json5"
json --json --overwrite "$DIR/convert.json5" >/dev/null 2>&1
if contains "$(cat "$DIR/convert.json5")" '"a"'; then
    pass "json --json --overwrite converts in place"
else
    fail "json --json --overwrite did not convert in place"
fi

#
#   --remove and --default
#
echo "Testing --remove..."
echo "{a: 1, b: 2}" > "$DIR/remove.json5"
OUTPUT=$(json --remove b "$DIR/remove.json5" 2>&1)
if [ $? -eq 0 ] && ! contains "$OUTPUT" "b"; then
    pass "json --remove drops the property"
else
    fail "json --remove did not drop the property"
fi

OUTPUT=$(json --remove missing "$DIR/remove.json5" 2>&1)
if [ $? -ne 0 ]; then
    pass "json --remove fails on a missing property"
else
    fail "json --remove did not fail on a missing property"
fi

OUTPUT=$(json --noerror --remove missing "$DIR/remove.json5" 2>&1)
if [ $? -eq 0 ]; then
    pass "json --noerror --remove tolerates a missing property"
else
    fail "json --noerror --remove failed"
fi

OUTPUT=$(json --default fallback missing "$DIR/remove.json5" 2>&1)
if [ "$OUTPUT" = "fallback" ]; then
    pass "json --default supplies a value for a missing property"
else
    fail "json --default returned '$OUTPUT'"
fi

#
#   --blend
#
echo "Testing --blend..."
echo "{extra: 'blended'}" > "$DIR/inc.json5"
echo "{blend: ['inc.json5'], base: 1}" > "$DIR/blend.json5"
OUTPUT=$(json --blend "$DIR/blend.json5" 2>&1)
if [ $? -eq 0 ] && contains "$OUTPUT" "blended" && ! contains "$OUTPUT" "inc.json5"; then
    pass "json --blend merges the blend[] files and drops blend[]"
else
    fail "json --blend produced '$OUTPUT'"
fi

echo "{blend: ['no-such-include.json5']}" > "$DIR/blendbad.json5"
OUTPUT=$(json --blend "$DIR/blendbad.json5" 2>&1)
if [ $? -ne 0 ] && contains "$OUTPUT" "no-such-include.json5"; then
    pass "json --blend fails on a missing include and names it"
else
    fail "json --blend on a missing include produced '$OUTPUT'"
fi

#
#   --profile
#
echo "Testing --profile..."
cat > "$DIR/profile.json5" <<'EOJ'
{
    profile: 'dev',
    debug: false,
    conditional: {
        profile: {
            dev: { debug: true, level: 'verbose' },
            prod: { debug: false, level: 'quiet' }
        }
    }
}
EOJ
OUTPUT=$(json --profile dev debug "$DIR/profile.json5" 2>&1)
if [ "$OUTPUT" = "true" ]; then
    pass "json --profile dev merges the dev branch"
else
    fail "json --profile dev returned '$OUTPUT'"
fi

OUTPUT=$(json --profile prod debug "$DIR/profile.json5" 2>&1)
if [ "$OUTPUT" = "false" ]; then
    pass "json --profile prod merges the prod branch"
else
    fail "json --profile prod returned '$OUTPUT'"
fi

OUTPUT=$(json --profile prod level "$DIR/profile.json5" 2>&1)
if [ "$OUTPUT" = "quiet" ]; then
    pass "json --profile merges properties absent from the destination"
else
    fail "json --profile level returned '$OUTPUT'"
fi

OUTPUT=$(json --profile dev "$DIR/profile.json5" 2>&1)
if [ $? -eq 0 ] && ! contains "$OUTPUT" "conditional"; then
    pass "json --profile drops the conditional block once merged"
else
    fail "json --profile left the conditional block in '$OUTPUT'"
fi

echo "{settings: {debug: false}}" > "$DIR/noconditional.json5"
OUTPUT=$(json --profile dev settings.debug "$DIR/noconditional.json5" 2>&1)
if [ $? -eq 0 ] && [ "$OUTPUT" = "false" ]; then
    pass "json --profile is a no-op on a document with no conditional block"
else
    fail "json --profile on a plain document returned '$OUTPUT'"
fi

#
#   Output formats
#
echo "Testing --env, --export, --header and --keys..."
echo "{app: {name: 'demo', debug: true, port: 80}}" > "$DIR/fmt.json5"

OUTPUT=$(json --env app "$DIR/fmt.json5" 2>&1)
if contains "$OUTPUT" "NAME='demo'" && contains "$OUTPUT" "PORT=80"; then
    pass "json --env emits shell variables"
else
    fail "json --env emitted '$OUTPUT'"
fi

OUTPUT=$(json --env --export app "$DIR/fmt.json5" 2>&1)
if contains "$OUTPUT" "export "; then
    pass "json --export prefixes shell variables"
else
    fail "json --export emitted '$OUTPUT'"
fi

OUTPUT=$(json --header app "$DIR/fmt.json5" 2>&1)
if contains "$OUTPUT" "#define" && contains "$OUTPUT" "DEBUG 1"; then
    pass "json --header emits C defines"
else
    fail "json --header emitted '$OUTPUT'"
fi

OUTPUT=$(json --keys app "$DIR/fmt.json5" 2>&1)
if contains "$OUTPUT" "name" && contains "$OUTPUT" "port" && ! contains "$OUTPUT" "demo"; then
    pass "json --keys emits key names only"
else
    fail "json --keys emitted '$OUTPUT'"
fi

echo "{list: ['one', 'two']}" > "$DIR/array.json5"
OUTPUT=$(json --keys list "$DIR/array.json5" 2>&1)
if contains "$OUTPUT" "one" && contains "$OUTPUT" "two"; then
    pass "json --keys emits array element values"
else
    fail "json --keys on an array emitted '$OUTPUT'"
fi

OUTPUT=$(json --env "$DIR/fmt.json5" 2>&1)
if contains "$OUTPUT" "NAME='demo'"; then
    pass "json --env without a property emits the whole document"
else
    fail "json --env whole document emitted '$OUTPUT'"
fi

echo "Testing --js..."
OUTPUT=$(json --js "$DIR/fmt.json5" 2>&1)
if contains "$OUTPUT" "export default"; then
    pass "json --js emits an ES module default export"
else
    fail "json --js emitted '$OUTPUT'"
fi
OUTPUT=$(json --js app.name "$DIR/fmt.json5" 2>&1)
if contains "$OUTPUT" "export default"; then
    pass "json --js emits a queried property as a default export"
else
    fail "json --js query emitted '$OUTPUT'"
fi

#
#   Formatting options
#
echo "Testing formatting options..."
for OPT in "--one" "--compact" "--encode" "--single" "--double" "--indent 2" "--length 40"; do
    OUTPUT=$(json $OPT "$DIR/fmt.json5" 2>&1)
    if [ $? -eq 0 ] && contains "$OUTPUT" "demo"; then
        pass "json $OPT produces output"
    else
        fail "json $OPT produced '$OUTPUT'"
    fi
done

echo "Testing --expand..."
cat > "$DIR/expand.json5" <<'EOJ'
{name: 'world', greeting: 'hello ${name}'}
EOJ
OUTPUT=$(json --expand "$DIR/expand.json5" 2>&1)
if contains "$OUTPUT" "hello world"; then
    pass "json --expand expands \${} references"
else
    fail "json --expand emitted '$OUTPUT'"
fi

#
#   --strict
#
echo "Testing --strict..."
echo '{"a": 1}' > "$DIR/strict.json"
if json --strict a "$DIR/strict.json" >/dev/null 2>&1; then
    pass "json --strict accepts standard JSON"
else
    fail "json --strict rejected standard JSON"
fi
if json --strict a "$DIR/in.json5" >/dev/null 2>&1; then
    fail "json --strict accepted JSON5 input"
else
    pass "json --strict rejects JSON5 input"
fi

#
#   stdin
#
echo "Testing --stdin..."
OUTPUT=$(echo "{a: 'piped'}" | json --json --stdin 2>&1)
if contains "$OUTPUT" '"a"'; then
    pass "json --stdin converts piped input"
else
    fail "json --stdin conversion returned '$OUTPUT'"
fi

OUTPUT=$(echo "{a: 'piped'}" | json --env --stdin 2>&1)
if contains "$OUTPUT" "A='piped'"; then
    pass "json --stdin honours the output format"
else
    fail "json --stdin --env returned '$OUTPUT'"
fi

OUTPUT=$(echo "{a: 'piped', b: {c: 2}}" | json --stdin a 2>&1)
if [ "$OUTPUT" = "piped" ]; then
    pass "json --stdin queries a property of piped input"
else
    fail "json --stdin query returned '$OUTPUT'"
fi

OUTPUT=$(echo "{a: 'piped', b: {c: 2}}" | json --stdin b.c 2>&1)
if [ "$OUTPUT" = "2" ]; then
    pass "json --stdin queries a dotted property of piped input"
else
    fail "json --stdin dotted query returned '$OUTPUT'"
fi

OUTPUT=$(echo "{a: 1}" | json --stdin a=2 2>&1)
if contains "$OUTPUT" "2"; then
    pass "json --stdin assigns to piped input"
else
    fail "json --stdin assignment returned '$OUTPUT'"
fi

OUTPUT=$(json a "$DIR/in.json5" 2>&1)
if [ "$OUTPUT" = "1" ]; then
    pass "a file argument is still the file when --stdin is absent"
else
    fail "file query returned '$OUTPUT'"
fi

echo "Testing --verbose and --debug..."
for OPT in --verbose --debug; do
    OUTPUT=$(json $OPT a "$DIR/in.json5" 2>/dev/null)
    if [ "$OUTPUT" = "1" ]; then
        pass "json $OPT still queries correctly"
    else
        fail "json $OPT returned '$OUTPUT'"
    fi
done

rm -rf "$DIR"

# Summary
echo ""
if [ $FAIL -eq 0 ]; then
    echo "All tests passed"
    exit 0
else
    echo "$FAIL test(s) failed"
    exit 1
fi
