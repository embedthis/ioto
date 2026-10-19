#!/bin/bash
#
# cleanup.sh - Clean up any stray temporary test files
#
# This script removes any temporary files that may have been left behind
# by interrupted tests. Run this if tests are killed mid-execution.
#

echo in PREP >&2
echo PATH is $PATH >&2
find ../../build >&2

type json