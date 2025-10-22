#!/bin/bash
#
# cleanup.sh - Clean up any stray temporary test files
#
# This script removes any temporary files that may have been left behind
# by interrupted tests. Run this if tests are killed mid-execution.
#

echo "Cleaning up temporary test files..."

# Remove test files from /tmp
rm -f /tmp/json-test-*.json 2>/dev/null
rm -f /tmp/json-test-*.json5 2>/dev/null
rm -f /tmp/json-test-*.txt 2>/dev/null
rm -f /tmp/json-test-in-*.json 2>/dev/null
rm -f /tmp/json-test-in-*.json5 2>/dev/null
rm -f /tmp/json-test-out-*.json 2>/dev/null
rm -f /tmp/json-test-out-*.txt 2>/dev/null

echo "Cleanup complete"
