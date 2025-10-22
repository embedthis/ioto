#!/usr/bin/env bash
#
#   setup.sh - TestMe setup script to start ioto
#

set -m

echo PATH ${PATH}
pwd
ls ../../bin
echo BUILD BIN
find ../../build
env

ENDPOINT=`json 'listen[0]' ./state/config/web.json5`

if curl -s "${ENDPOINT}" >/dev/null 2>&1; then
    echo "Ioto is already running"
    sleep 999999 &
    PID=$!
else
    ioto -v &
    PID=$!
fi

cleanup() {
    kill $PID >/dev/null 2>&1
    exit 0
}

trap cleanup SIGINT SIGTERM SIGQUIT EXIT

wait $PID

