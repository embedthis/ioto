#!/bin/bash
#
#   setup.sh - TestMe setup script to start web
#

set -m

# mosquitto -c /opt/homebrew/etc/mosquitto/mosquitto.conf &

sleep 999999 &
PID=$!

cleanup() {
    kill $PID >/dev/null 2>&1
    exit 0
}

trap cleanup SIGINT SIGTERM SIGQUIT EXIT

wait $PID