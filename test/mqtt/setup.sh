#!/bin/bash
#
#   setup.sh - TestMe setup script to run a local mosquitto broker for the test suite
#
#   The tests connect to localhost on MQTT_PORT (default 1883). If a broker is already listening
#   there, it is used as-is. Otherwise mosquitto is started on the IPv4 loopback with anonymous
#   access and stopped when TestMe terminates this script.
#
#   TestMe spawns this script with an unlimited open-files limit, and mosquitto sizes a table from
#   that limit at startup, so it fails with "Error: Out of memory" and exits. The limit is capped
#   before the broker starts.
#

PORT=${MQTT_PORT:-1883}
CONF="${TMPDIR:-/tmp}/mosquitto-${PORT}.conf"
LOG="${TMPDIR:-/tmp}/mosquitto-${PORT}.log"

cleanup() {
    if [ -n "${PID}" ] ; then
        kill "${PID}" 2>/dev/null
    fi
    rm -f "${CONF}"
    exit 0
}
trap cleanup SIGINT SIGTERM SIGQUIT EXIT

if nc -z localhost "${PORT}" 2>/dev/null ; then
    echo "Using existing MQTT broker on port ${PORT}"
    while true ; do sleep 3600 ; done
fi

if ! command -v mosquitto >/dev/null 2>&1 ; then
    echo "mosquitto not found: install it (brew install mosquitto) or start a broker on port ${PORT}" >&2
    while true ; do sleep 3600 ; done
fi

ulimit -n 1024
printf 'listener %s 127.0.0.1\nallow_anonymous true\n' "${PORT}" > "${CONF}"
mosquitto -c "${CONF}" > "${LOG}" 2>&1 &
PID=$!
wait "${PID}"
if [ -f "${LOG}" ] ; then
    cat "${LOG}" >&2
fi
