#
#   get-item.sh -- Get an item from the Store table
#
#   Environment credentials
#      IOTO_API
#      IOTO_TOKEN
#

url -s hHbB POST ${IOTO_API}/tok/generic/get \
    "Authorization: bearer ${IOTO_TOKEN}" \
    "{_type: 'Store', accountId: '${IOTO_ACCOUNT}', deviceId: '${IOTO_DEVICE}'}"
