#!/bin/bash
#
#   env.sh - Set the test environment and echo to stdout.
#   This includes credentials for the test
#

if [ "${GITHUB_ACTIONS}" != "true" ] ; then

    if [ -f .creds.sh ] ; then
        . .creds.sh
    else
        #
        #   Set these to your Builder product ID
        #
        IOTO_PRODUCT=""

        #
        #   Get these values from your device app account settings
        #
        IOTO_ACCOUNT=""
        IOTO_CLOUD=""

        #
        #   Leave your device ID empty to auto allocate when the agent starts
        #
        IOTO_ID=""
    fi
fi

ENDPOINT=`json 'listen[0]' ./state/config/web.json5`/api/test

export ENDPOINT IOTO_ACCOUNT IOTO_CLOUD IOTO_ID IOTO_PRODUCT

echo "ENDPOINT=${ENDPOINT}"
echo "IOTO_ACCOUNT=${IOTO_ACCOUNT}"
echo "IOTO_CLOUD=${IOTO_CLOUD}"
echo "IOTO_ID=${IOTO_ID}"
echo "IOTO_PRODUCT=${IOTO_PRODUCT}"
exit 0
