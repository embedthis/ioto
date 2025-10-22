#!/bin/bash
#
#   prep.sh - TestMe prep script for fuzzing library
#
#   Compile the fuzzLib library before tests run
#

set -e

#
# Future
#   build/${PLATFORM}-${ARCH}-${PROFILE}/bin
#
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# Create .testme directory if it doesn't exist
mkdir -p .testme

# Find build directory
BUILD_DIR=$(ls -d ../../build/*/inc 2>/dev/null | head -1 | sed 's|/inc||')
if [ -z "$BUILD_DIR" ]; then
    echo "Error: Cannot find build directory"
    exit 1
fi

# Compile fuzzLib library with comprehensive strict warning flags
echo "Compiling fuzzLib..."
gcc -c -g -O0 -arch arm64 -fPIC -fstack-protector --param=ssp-buffer-size=4 \
    -Wall -Wextra -Wno-unused-parameter -Wno-unused-function \
    -Wno-unused-result -Wshorten-64-to-32 -Wno-unknown-warning-option \
    -Wformat -Wformat-security -Wsign-compare -Wsign-conversion \
    -Wcast-align -Wcast-qual -Wpointer-arith \
    -Wstrict-overflow=2 -Wshadow -Wwrite-strings \
    -DEMBEDTHIS=1 -DME_DEBUG=1 -D_REENTRANT -DPIC \
    -I. -I.. -I${BUILD_DIR}/inc -I${HOME}/.local/include \
    fuzzLib.c -o .testme/fuzzLib.o

# Create static library
echo "Creating libfuzz.a..."
ar rcs .testme/libfuzz.a .testme/fuzzLib.o

echo "✓ libfuzz.a created in .testme/"
