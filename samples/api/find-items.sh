#
#   find-items.sh -- Find items in the Device table
#
#   Environment credentials
#      IOTO_API
#      IOTO_TOKEN
#

url -s hHbB POST ${IOTO_API}/tok/generic/find \
    "Authorization: bearer ${IOTO_TOKEN}" \
    "{_type: 'Device', accountId: '${IOTO_ACCOUNT}'}"
