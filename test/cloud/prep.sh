#!/usr/bin/env bash
#
#   prep.sh - TestMe prep script
#

mkdir -p state/config state/db state/certs

for n in aws roots test ; do
    cp -r ../../state/certs/${n}.* ./state/certs
done