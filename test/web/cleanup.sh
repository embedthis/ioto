#!/usr/bin/env bash
#
#   TestMe cleanup script
#

echo SUCCESS: ${TESTME_SUCCESS}
echo SUCCESS: ${TESTME_SUCCESS} >&2
if [ "${TESTME_SUCCESS}" = "1" ]; then
    rm -f web.log
else
    cat web.log
fi

rm -f tmp/*
rm -f site/range-test-write.txt
rm -rf site/upload/*
