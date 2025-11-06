#!/usr/bin/env bash
#
#   prep.sh - TestMe prep script for fuzzing library
#
#   Compile the fuzz library before tests run
#

rm -fr ../site/upload/
mkdir -p .testme ../site/upload
cp -r ../../../certs ../../certs

#
#  Build the server with ASAN
#
#CFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
#LDFLAGS='-fsanitize=address,undefined' \
#make -C ../../.. clean build
#
#echo "Ioto rebuilt with ASAN"