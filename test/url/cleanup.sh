#!/bin/bash
#
#   TestMe cleanup script
#

if [ "${TESTME_SUCCESS}" = "1" ]; then
    rm -f web.log web.out upload-acl.log
else
    cat web.log web.out 2>/dev/null
fi
rm -f tmp/*.tmp

# Remove PUT uploads while preserving tracked fixtures.
find site/upload -mindepth 1 -type f ! -name .dummy ! -name temp.dat ! -name test-put.txt ! -name upload.html -delete 2>/dev/null
