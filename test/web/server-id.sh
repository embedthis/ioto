#!/usr/bin/env bash
#
#   server-id.sh - Identify this suite's web server. Sourced by prep.sh and setup.sh, not run.
#
#   The probe has to identify THIS checkout's server, not merely a server: any other checkout's
#   document root also has an index.html, so a bare 200 adopted a foreign server on the port and
#   ran the whole suite against it. prep.sh writes the test directory's own path into
#   site/${PROBE}, so a second checkout, or the agent's vendored copy of this suite, does not
#   match, while a server a developer started by hand in this checkout still does.
#

#   Must match the healthcheck document in testme.json5
PROBE=suite-id.txt
SUITE_ID=$(pwd -P)

if [ -f "web.json5" ]; then
    CONFIG="web.json5"
elif [ -f "../web.json5" ]; then
    CONFIG="../web.json5"
else
    echo "Error: Cannot find web.json5" >&2
    exit 1
fi

ENDPOINT=`json 'web.listen[0]' $CONFIG`
if [ -z "$ENDPOINT" ]; then
    echo "Error: Cannot get endpoint from $CONFIG" >&2
    exit 1
fi

#
#   A server already serving THIS suite. curl exits 0 on a 404 or any other error response, so
#   neither its exit status nor a 200 is enough: the identity document is compared.
#
probe() {
    [ "$(curl -sf --max-time 2 ${ENDPOINT}/${PROBE} 2>/dev/null)" = "${SUITE_ID}" ]
}

#
#   Something is answering on the port, whatever it is. Without -f this succeeds on any HTTP
#   response and fails only when nothing accepts the connection.
#
occupied() {
    curl -s -o /dev/null --max-time 2 ${ENDPOINT}/ 2>/dev/null
}

#
#   A foreign server holds the port. Adopting it runs the whole suite against a foreign document
#   root, which surfaces as a dozen unrelated assertion failures rather than as the port conflict
#   it is, so name the cause instead.
#
refuseForeign() {
    echo "Error: another web server is listening on ${ENDPOINT}." >&2
    echo "       It did not serve ${PROBE} as ${SUITE_ID}, so it is not this suite's server." >&2
    echo "       Stop it and re-run; no test was executed against it." >&2
    lsof -nP -iTCP:${ENDPOINT##*:} -sTCP:LISTEN >&2 2>/dev/null
}
