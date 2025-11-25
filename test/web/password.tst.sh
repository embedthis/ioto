#!/bin/bash
#
#   password.tst.sh - Unit tests for password utility
#
#   Copyright (c) All Rights Reserved. See details at the end of the file.
#

PASSWORD=password

# Test SHA-256 (default algorithm)
echo "Testing SHA-256 password generation..."
output=$($PASSWORD --password "secret123" --realm "Test Realm" alice 2>&1)
if ! echo "$output" | grep -q "password: 'SHA256:[0-9a-f]\{64\}'"; then
    echo "FAIL: SHA-256 password should have SHA256: prefix and 64 hex characters"
    exit 1
fi
if ! echo "$output" | grep -q "realm 'Test Realm'"; then
    echo "FAIL: Output should mention realm"
    exit 1
fi
if ! echo "$output" | grep -q "alice:"; then
    echo "FAIL: Output should contain username"
    exit 1
fi
echo "PASS: SHA-256 password generation"

# Test MD5 algorithm
echo "Testing MD5 password generation..."
output=$($PASSWORD --algorithm md5 --password "secret123" --realm "Test Realm" bob 2>&1)
if ! echo "$output" | grep -q "password: 'MD5:[0-9a-f]\{32\}'"; then
    echo "FAIL: MD5 password should have MD5: prefix and 32 hex characters"
    exit 1
fi
if ! echo "$output" | grep -q "md5"; then
    echo "FAIL: Output should mention MD5 algorithm"
    exit 1
fi
echo "PASS: MD5 password generation"

# Test SHA-512 algorithm
echo "Testing SHA-512 password generation..."
output=$($PASSWORD --algorithm sha512 --password "secret123" --realm "Test Realm" charlie 2>&1)
if ! echo "$output" | grep -q "password: 'SHA512:[0-9a-f]\{128\}'"; then
    echo "FAIL: SHA-512 password should have SHA512: prefix and 128 hex characters"
    exit 1
fi
if ! echo "$output" | grep -q "sha512"; then
    echo "FAIL: Output should mention SHA-512 algorithm"
    exit 1
fi
echo "PASS: SHA-512 password generation"

# Test default realm
echo "Testing default realm..."
output=$($PASSWORD --password "secret123" david 2>&1)
if ! echo "$output" | grep -q "example.com"; then
    echo "FAIL: Should use default realm 'example.com'"
    exit 1
fi
echo "PASS: Default realm"

# Test invalid algorithm
echo "Testing invalid algorithm..."
if $PASSWORD --algorithm invalid --password "secret123" eve 2>&1; then
    echo "FAIL: Should reject invalid algorithm with non-zero exit code"
    exit 1
else
    echo "PASS: Invalid algorithm rejected"
fi

# Test hash consistency - same input should produce same output
echo "Testing hash consistency..."
hash1=$($PASSWORD --algorithm sha256 --password "test" --realm "TestRealm" user1 2>&1 | grep "password:" | sed "s/.*password: '\\(SHA256:[^']*\\)'.*/\\1/")
hash2=$($PASSWORD --algorithm sha256 --password "test" --realm "TestRealm" user1 2>&1 | grep "password:" | sed "s/.*password: '\\(SHA256:[^']*\\)'.*/\\1/")
if [ "$hash1" != "$hash2" ]; then
    echo "FAIL: Same input should produce same hash"
    echo "Hash 1: $hash1"
    echo "Hash 2: $hash2"
    exit 1
fi
echo "PASS: Hash consistency"

# Test different passwords produce different hashes
echo "Testing hash uniqueness..."
hash1=$($PASSWORD --algorithm sha256 --password "password1" --realm "TestRealm" user1 2>&1 | grep "password:" | sed "s/.*password: '\\(SHA256:[^']*\\)'.*/\\1/")
hash2=$($PASSWORD --algorithm sha256 --password "password2" --realm "TestRealm" user1 2>&1 | grep "password:" | sed "s/.*password: '\\(SHA256:[^']*\\)'.*/\\1/")
if [ "$hash1" = "$hash2" ]; then
    echo "FAIL: Different passwords should produce different hashes"
    exit 1
fi
echo "PASS: Hash uniqueness"

# Test different realms produce different hashes
echo "Testing realm impact on hash..."
hash1=$($PASSWORD --algorithm sha256 --password "test" --realm "Realm1" user1 2>&1 | grep "password:" | sed "s/.*password: '\\(SHA256:[^']*\\)'.*/\\1/")
hash2=$($PASSWORD --algorithm sha256 --password "test" --realm "Realm2" user1 2>&1 | grep "password:" | sed "s/.*password: '\\(SHA256:[^']*\\)'.*/\\1/")
if [ "$hash1" = "$hash2" ]; then
    echo "FAIL: Different realms should produce different hashes"
    exit 1
fi
echo "PASS: Realm impact on hash"

# Test different usernames produce different hashes
echo "Testing username impact on hash..."
hash1=$($PASSWORD --algorithm sha256 --password "test" --realm "TestRealm" alice 2>&1 | grep "password:" | sed "s/.*password: '\\(SHA256:[^']*\\)'.*/\\1/")
hash2=$($PASSWORD --algorithm sha256 --password "test" --realm "TestRealm" bob 2>&1 | grep "password:" | sed "s/.*password: '\\(SHA256:[^']*\\)'.*/\\1/")
if [ "$hash1" = "$hash2" ]; then
    echo "FAIL: Different usernames should produce different hashes"
    exit 1
fi
echo "PASS: Username impact on hash"

echo ""
echo "All password utility tests passed!"

#
#   Copyright (c) Embedthis Software. All Rights Reserved.
#   This is proprietary software and requires a commercial license from the author.
#
