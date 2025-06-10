#
#   device-command.sh -- Invoke the DeviceCommand automation
#
#   Environment credentials
#      IOTO_API
#      IOTO_TOKEN
#

url -s hHbB POST ${IOTO_API}/tok/action/invoke \
    "Authorization: bearer ${IOTO_TOKEN}" \
    "{name: 'DeviceCommand', context: {deviceId: 'DEV1234560', program: 'date', parameters: '-u'}}"
