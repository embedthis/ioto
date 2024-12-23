#!/bin/bash
#
#  uninstall: Ioto uninstall script
#
#  Copyright (c) Embedthis Software. All Rights Reserved.
#
#  Usage: uninstall
#
################################################################################

unset CDPATH IFS
PATH="$PATH:/sbin:/usr/sbin:/usr/local/bin"
export PATH

###############################################################################

cd /
echo -e "\nIoto Uninstall\n"

if [ `id -u` != "0" ] ; then
    echo "You must be root to remove this product."
    exit 1
fi

if [ -f /var/run/ioto.pid ] ; then
    kill `cat /var/run/ioto.pid` 2>&1
fi
echo -e "Removing files ..."

rm -f /var/log/ioto.log*
rm -fr /var/lib/ioto
rm -fr /etc/ioto
rm -f /usr/local/bin/ioto
rm -rf /usr/local/lib/ioto

echo -e "\nIoto uninstall complete"
exit 0
