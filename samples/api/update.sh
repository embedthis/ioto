#
#   update.sh -- Check for software updates
#
#   Environment credentials
#      IOTO_API
#      IOTO_TOKEN
#      IOTO_PRODUCT
#

url -s hHbB POST ${IOTO_API}/tok/provision/update \
    "Authorization: bearer ${IOTO_TOKEN}" \
    "{id: 'TEST000001', product: '${IOTO_PRODUCT}', version: '2.1.3'}"

