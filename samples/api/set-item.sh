#
#   set-item.sh -- Set an item in the Store table
#
#   Environment credentials
#      IOTO_API
#      IOTO_TOKEN
#

KEY=set-value
VALUE=`date +%s`

url -s hHbB POST ${IOTO_API}/tok/generic/update?exists \
    "Authorization: bearer ${IOTO_TOKEN}" \
    "{_type: 'Store', accountId: '${IOTO_ACCOUNT}', deviceId: '${IOTO_DEVICE}', key: '${KEY}', value: '${VALUE}', type: 'string'}"
