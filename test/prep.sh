#!/usr/bin/env bash
#
#   prep.sh - Ioto global TestMe prep script
#

app=`json app ../state/config/ioto.json5`

if [ "$app" != "unit" ] ; then
    echo "Ioto not configured for unit tests. Currently selected \"$app\" app, need \"unit\" app" >&2
    exit 1
fi

if [ ! -d certs ] ; then
    mkdir -p certs
    cp ../certs/*.crt certs
    cp ../certs/*.key certs
fi

exit 0