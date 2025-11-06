#!/usr/bin/env bash
#
#   prep.sh - TestMe prep script for fuzzing library
#
#   Compile the fuzz library before tests run
#

mkdir -p .testme
cp -r ../../../certs ../../certs

#
#  Build the server with ASAN
#
#CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
#LDFLAGS='-fsanitize=address,undefined' \
#make -C ../../.. clean build
#
#echo "Ioto rebuilt with ASAN"