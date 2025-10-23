#!/usr/bin/env bash
#
#   skip.sh - TestMe skip script
#

if [ -f .creds.sh ] ; then
    . .creds.sh
fi

if [ "${IOTO_CLOUD}" = "" -o "${IOTO_CLOUD}" = "PUT-YOUR-CLOUD-ID-HERE" ] ; then
    echo "IOTO_CLOUD is not set, skipping test"
    echo "Must set IOTO_CLOUD, IOTO_ACCOUNT, and IOTO_PRODUCT environment variables"
    exit 1
fi
exit 0
