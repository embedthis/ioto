#!/usr/bin/env bash
#
#   prep.sh - TestMe prep script
#

app=`json app ../state/config/ioto.json5`

if [ "$app" != "unit" ] ; then
    echo "Ioto not configured for unit tests. Currently selected \"$app\" app, need \"unit\" app" >&2
    exit 1
fi

ID="TEST${TESTME_OS}"
json id=${ID} ../state/config/device.json5