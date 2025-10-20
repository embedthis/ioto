#!/usr/bin/env bash
#
#   setup.sh - TestMe setup script to start ioto
#

set -m
echo IN SETUP
pwd
find state

set -x

if curl -s http://localhost:9090/ >/dev/null 2>&1; then
    echo "Ioto is already running on port 9090"
    sleep 999999 &
else
    echo "Starting ioto"
    ioto --trace ./log.txt &
fi
PID=$!

cleanup() {
    kill $PID >/dev/null 2>&1
    cat ./log.txt
    echo "Killed ioto"
    exit 0
}

trap cleanup SIGINT SIGTERM SIGQUIT EXIT

wait $PID

