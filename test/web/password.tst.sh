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
    echo "✗ SHA-256 password should have SHA256: prefix and 64 hex characters"
    exit 1
fi
if ! echo "$output" | grep -q "realm 'Test Realm'"; then
    echo "✗ Output should mention realm"
    exit 1
fi
if ! echo "$output" | grep -q "alice:"; then
    echo "✗ Output should contain username"
    exit 1
fi
echo "✓ SHA-256 password generation"

# Test MD5 algorithm
echo "Testing MD5 password generation..."
output=$($PASSWORD --algorithm md5 --password "secret123" --realm "Test Realm" bob 2>&1)
if ! echo "$output" | grep -q "password: 'MD5:[0-9a-f]\{32\}'"; then
    echo "✗ MD5 password should have MD5: prefix and 32 hex characters"
    exit 1
fi
if ! echo "$output" | grep -q "md5"; then
    echo "✗ Output should mention MD5 algorithm"
    exit 1
fi
echo "✓ MD5 password generation"

# Test SHA-512 algorithm
echo "Testing SHA-512 password generation..."
output=$($PASSWORD --algorithm sha512 --password "secret123" --realm "Test Realm" charlie 2>&1)
if ! echo "$output" | grep -q "password: 'SHA512:[0-9a-f]\{128\}'"; then
    echo "✗ SHA-512 password should have SHA512: prefix and 128 hex characters"
    exit 1
fi
if ! echo "$output" | grep -q "sha512"; then
    echo "✗ Output should mention SHA-512 algorithm"
    exit 1
fi
echo "✓ SHA-512 password generation"

# Test default realm matches the server's default realm, "web"
echo "Testing default realm..."
output=$($PASSWORD --password "secret123" david 2>&1)
if ! echo "$output" | grep -q "realm 'web'"; then
    echo "✗ Should use the server's default realm 'web'"
    exit 1
fi
if command -v sha256sum >/dev/null; then
    SHA256="sha256sum"
else
    SHA256="shasum -a 256"
fi
expected="SHA256:$(printf '%s' 'david:web:secret123' | $SHA256 | cut -d' ' -f1)"
if ! echo "$output" | grep -q "password: '$expected'"; then
    echo "✗ Default-realm hash should be H(david:web:secret123)"
    exit 1
fi
echo "✓ Default realm"

# Test bcrypt prints a database record
echo "Testing bcrypt database record..."
output=$($PASSWORD --cipher blowfish --realm web --password "secret123" admin 2>&1)
if ! echo "$output" | grep -q "^{ username: 'admin', password: 'BF1:[^']*', role: 'user' }$"; then
    echo "✗ Bcrypt should print a db.json5 User record"
    echo "$output"
    exit 1
fi
echo "✓ Bcrypt database record"

# Test invalid algorithm
echo "Testing invalid algorithm..."
if $PASSWORD --algorithm invalid --password "secret123" eve 2>&1; then
    echo "✗ Should reject invalid algorithm with non-zero exit code"
    exit 1
else
    echo "✓ Invalid algorithm rejected"
fi

# Test hash consistency - same input should produce same output
echo "Testing hash consistency..."
hash1=$($PASSWORD --algorithm sha256 --password "test" --realm "TestRealm" user1 2>&1 | grep "password:" | sed "s/.*password: '\\(SHA256:[^']*\\)'.*/\\1/")
hash2=$($PASSWORD --algorithm sha256 --password "test" --realm "TestRealm" user1 2>&1 | grep "password:" | sed "s/.*password: '\\(SHA256:[^']*\\)'.*/\\1/")
if [ "$hash1" != "$hash2" ]; then
    echo "✗ Same input should produce same hash"
    echo "Hash 1: $hash1"
    echo "Hash 2: $hash2"
    exit 1
fi
echo "✓ Hash consistency"

# Test different passwords produce different hashes
echo "Testing hash uniqueness..."
hash1=$($PASSWORD --algorithm sha256 --password "password1" --realm "TestRealm" user1 2>&1 | grep "password:" | sed "s/.*password: '\\(SHA256:[^']*\\)'.*/\\1/")
hash2=$($PASSWORD --algorithm sha256 --password "password2" --realm "TestRealm" user1 2>&1 | grep "password:" | sed "s/.*password: '\\(SHA256:[^']*\\)'.*/\\1/")
if [ "$hash1" = "$hash2" ]; then
    echo "✗ Different passwords should produce different hashes"
    exit 1
fi
echo "✓ Hash uniqueness"

# Test different realms produce different hashes
echo "Testing realm impact on hash..."
hash1=$($PASSWORD --algorithm sha256 --password "test" --realm "Realm1" user1 2>&1 | grep "password:" | sed "s/.*password: '\\(SHA256:[^']*\\)'.*/\\1/")
hash2=$($PASSWORD --algorithm sha256 --password "test" --realm "Realm2" user1 2>&1 | grep "password:" | sed "s/.*password: '\\(SHA256:[^']*\\)'.*/\\1/")
if [ "$hash1" = "$hash2" ]; then
    echo "✗ Different realms should produce different hashes"
    exit 1
fi
echo "✓ Realm impact on hash"

# Test different usernames produce different hashes
echo "Testing username impact on hash..."
hash1=$($PASSWORD --algorithm sha256 --password "test" --realm "TestRealm" alice 2>&1 | grep "password:" | sed "s/.*password: '\\(SHA256:[^']*\\)'.*/\\1/")
hash2=$($PASSWORD --algorithm sha256 --password "test" --realm "TestRealm" bob 2>&1 | grep "password:" | sed "s/.*password: '\\(SHA256:[^']*\\)'.*/\\1/")
if [ "$hash1" = "$hash2" ]; then
    echo "✗ Different usernames should produce different hashes"
    exit 1
fi
echo "✓ Username impact on hash"

echo ""
echo "All password utility tests passed!"

#
#   Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
#   This is proprietary software and requires a commercial license from the author.
#
