#!/usr/bin/env bash
#
#   TestMe cleanup script
#

echo SUCCESS: ${TESTME_SUCCESS}
echo SUCCESS: ${TESTME_SUCCESS} >&2
if [ "${TESTME_SUCCESS}" = "1" ]; then
    rm -f web.log web.out upload-acl.log
else
    cat web.log web.out 2>/dev/null
fi

rm -f tmp/*
rm -rf site/upload/*
rm -rf site-evil-*/
