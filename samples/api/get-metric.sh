#
#   get-metric.sh -- Get a metric
#
#   Environment credentials
#      IOTO_API
#      IOTO_TOKEN
#

url -s hHbB POST ${IOTO_API}/tok/metric/fetch \
    "Authorization: bearer ${IOTO_TOKEN}" \
    "{ \
        accountId: '${IOTO_ACCOUNT}', \
        deviceId: '${IOTO_DEVICE}', \
        items: [{ \
            cloudId: ${IOTO_CLOUD}, \
            namespace: 'Embedthis/Device', \
            metric: 'RANDOM',
            dimensions: { Device: '${IOTO_DEVICE}'}, \
            statistic: 'current', \
            period: 86400, \
            accumulate: true \
        }] \
    }"


