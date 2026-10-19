#!/usr/bin/env bash
#
#   agent-prep.sh - Build the state directory a scenario group runs the agent from
#
#   Runs with the group directory (test/soak or test/stress) as the working directory, before
#   agent-setup.sh starts the agent there. The group's testme.json5 names it as the prep service
#   and sets TM_HTTP and TM_HTTPS, which are the only inputs.
#
#   The agent needs the same layout bin/prepare builds under apps/unit: state/config holding the
#   configuration, state/db for the database, state/site for the documents. Configuration comes
#   from test/utils/agent, users and device identity from apps/unit, and the blended schema from
#   apps/unit/state/config - which only exists after "make", so its absence is reported rather
#   than worked around.
#
#   The database is emptied on every run so each soak starts from the same baseline: the users
#   are re-seeded by the unit app on startup, and the Churn table refills during warm-up.
#
set -e

UTILS="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$UTILS/../.." && pwd)"
UNIT="$ROOT/apps/unit"

: "${TM_HTTP:?TM_HTTP must be set in the group's testme.json5 environment}"
: "${TM_HTTPS:?TM_HTTPS must be set in the group's testme.json5 environment}"

if [ ! -f "$UNIT/state/config/schema.json5" ] ; then
    echo "Error: $UNIT/state/config/schema.json5 is missing. Run make first." >&2
    exit 1
fi

mkdir -p state/config state/db state/site/size state/site/upload state/tmp
rm -f state/db/* state/tmp/* state/site/upload/*

cp "$UTILS/agent/ioto.json5" state/config/ioto.json5
#   The failure limit is off unless the group sets TM_MAX_FAILURES: soak and stress fail logins on
#   purpose, all from one address, and would otherwise lock their own fixtures out.
sed -e "s|@HTTP@|${TM_HTTP}|g" -e "s|@HTTPS@|${TM_HTTPS}|g" -e "s|@MAX_FAILURES@|${TM_MAX_FAILURES:-0}|g" \
    "$UTILS/agent/web.json5" > state/config/web.json5
#
#   The scenario accounts live here, with the rest of this group's configuration, and not in
#   apps/unit. A working credential in a shipped application template is a credential in every
#   device built from it; these exist only for the fixtures below to log in with.
#
cp "$UTILS/agent/db.json5" state/config/db.json5
cp "$UNIT/device.json5" state/config/device.json5
cp "$UNIT/state/config/schema.json5" state/config/schema.json5

if [ ! -f state/site/index.html ] ; then
    printf '<html><body><h1>Scenario test index</h1></body></html>\n' > state/site/index.html
fi

#
#   Sized documents, the same shape the web module's tests use: 50-byte lines and a trailer.
#   make-files is a no-op when the file exists.
#
"$ROOT/bin/make-files" 20 state/site/size/1K.txt >/dev/null
"$ROOT/bin/make-files" 205 state/site/size/10K.txt >/dev/null
"$ROOT/bin/make-files" 2050 state/site/size/100K.txt >/dev/null
"$ROOT/bin/make-files" 21000 state/site/size/1M.txt >/dev/null

#
#   The 5 GB document for stress/huge.tst.c, generated only at the depth that test runs at.
#   Real bytes rather than a sparse hole, so the server reads as many as it writes. Reused when
#   a previous run left one of the right size.
#
HUGE=state/site/huge.txt
HUGE_SIZE=5368709120
if [ "${TESTME_DEPTH:-0}" -ge 5 ] ; then
    if [ ! -f "$HUGE" ] || [ "$(stat -f %z "$HUGE" 2>/dev/null || stat -c %s "$HUGE")" -ne "$HUGE_SIZE" ] ; then
        echo "      [Generate] $HUGE (5 GB)"
        rm -f "$HUGE"
        yes '0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde' | head -c "$HUGE_SIZE" > "$HUGE"
    fi
fi
exit 0
