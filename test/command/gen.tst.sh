#!/usr/bin/env bash

mkdir -p .testme

id=`ioto --gen`
if [ $? -ne 0 ] ; then
    echo "✗ ioto -v --exit 2 failed" >&2
    exit 1
fi
if [ -z "$id" ] ; then
    echo "✗ ioto --gen failed" >&2
    exit 1
fi
if [ ${#id} -ne 10 ] ; then
    echo "✗ ioto --gen failed" >&2
    exit 1
fi

ioto --exit 2
if [ $? -ne 0 ] ; then
    echo "✗ ioto --exit 2 failed" >&2
    exit 1
fi

rm -f .testme/trace$$.log
ioto --trace .testme/$$trace.log --exit 2
if [ $? -ne 0 ] ; then
    echo "✗ ioto --trace .testme/$$trace.log --exit 2 failed" >&2
    exit 1
fi
if [ ! -f .testme/$$trace.log ] ; then
    echo "✗ ioto --trace failed" >&2
    exit 1
fi
echo "✓ Ioto command tests successful"
rm -f .testme/$$trace.log