#!/usr/bin/env bash
#
#   prep.sh - Ioto global TestMe prep script
#

#
#   Config is in apps/unit/state/config/ with the new per-app state layout
#
UNIT_CONFIG=../apps/unit/state/config/ioto.json5
LEGACY_CONFIG=../state/config/ioto.json5

if [ -f "${UNIT_CONFIG}" ] ; then
    app=`json app "${UNIT_CONFIG}"`
elif [ -f "${LEGACY_CONFIG}" ] ; then
    app=`json app "${LEGACY_CONFIG}"`
else
    app=""
fi

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