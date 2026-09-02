#!/usr/bin/env bash
#
#   check-config.sh - Check build configuration for test skip decisions
#
#   Sources build/config.sh and provides require_service function.
#
#   Usage from a test skip.sh:
#       . ../check-config.sh
#       require_service CLOUD
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_SH="${SCRIPT_DIR}/../build/config.sh"

if [ ! -f "${CONFIG_SH}" ] ; then
    echo "build/config.sh not found -- run 'make prep' first"
    exit 1
fi

. "${CONFIG_SH}"

require_service() {
    local service="$1"
    local var="SERVICES_${service}"
    local val="${!var}"
    if [ "${val}" != "1" ] ; then
        echo "SERVICES_${service} not compiled in, skipping test"
        exit 1
    fi
}
