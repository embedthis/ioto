#!/usr/bin/env bash
#
#   prep.sh - TestMe prep script
#

mkdir -p state/config state/db state/certs state/site

for n in aws roots test ; do
    cp -r ../../state/certs/${n}.* ./state/certs
done

if [ ! -f state/site/index.html ] ; then
    cat >state/site/index.html <<!EOF
<html><head><title>index.html</title></head>
<body>Hello /index.html</body>
</html> 
!EOF
fi