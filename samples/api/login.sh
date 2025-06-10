#
#   login.sh -- Test device cloud access using a Cognito AccessToken and then do Generic.find on Device table
#
#   Environment credentials
#      IOTO_COGNITO
#      IOTO_CLIENT_ID
#      IOTO_USERNAME
#      IOTO_PASSWORD
#      IOTO_API
#      IOTO_HOST

DATA=/tmp/data$$.tmp
OUTPUT=/tmp/output$$.tmp

set -x
cat >${DATA} <<!EOF
{
   "AuthParameters" : {
      "USERNAME": "${IOTO_USERNAME}",
      "PASSWORD": "${IOTO_PASSWORD}"
   },
   "AuthFlow": "USER_PASSWORD_AUTH",
   "ClientId": "${IOTO_CLIENT_ID}"
}
!EOF

#
#   Login 
#
url -s hHbB ${IOTO_COGNITO} \
    X-Amz-Target:AWSCognitoIdentityProviderService.InitiateAuth \
    Content-Type:application/x-amz-json-1.1 \
    @${DATA} >${OUTPUT}
if [ $? -ne 0 ] ; then
    echo "Failed to login" >&2 ; exit 2
fi

TOKEN=$(json AuthenticationResult.AccessToken ${OUTPUT})
rm -f ${DATA} ${OUTPUT}

#
#   Use token and get list of devices
#
url -s hHbB POST ${IOTO_API}/generic/find \
    "Origin:${IOTO_HOST}" \
    "Authorization:${TOKEN}" \
    "{_type:'Device'}"

