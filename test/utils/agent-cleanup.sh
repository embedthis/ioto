#!/usr/bin/env bash
#
#   agent-cleanup.sh - Tidy a scenario group after its run
#
#   The agent's log is kept whatever the verdict - it is gitignored and a soak's figures are
#   only useful beside the log that explains them - but its tail is printed on failure so the
#   reason is in the test output rather than in a file nobody opens.
#
if [ "${TESTME_SUCCESS}" != "1" ] && [ -s ioto.log ] ; then
    echo "----- ioto.log (last 40 lines)"
    tail -40 ioto.log
fi

rm -f state/tmp/* state/site/upload/* .pidfile
exit 0
