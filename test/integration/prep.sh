#!/usr/bin/env bash
#
#   prep.sh - TestMe prep script
#

make-files() {
    COUNT=$1
    OUTPUT=$2
    if [ ! -f $OUTPUT ] ; then
        echo "   [Create] $OUTPUT"
        > $OUTPUT
        while [ $COUNT -gt 0 ] ; 
        do
            echo 0123456789012345678901234567890123456789012345678 >>$OUTPUT
            COUNT=$((COUNT - 1))
        done
        echo END OF DOCUMENT >>$OUTPUT
    fi
}

mkdir -p ./state/config ./state/db ./state/certs ./state/site

if [ ! -f ./state/certs/test.crt ] ; then
    mkdir -p ./state/certs
    cp ../../certs/*.crt ./state/certs
    cp ../../certs/*.key ./state/certs
fi

if [ ! -f state/site/1K.txt ] ; then
    cat >state/site/index.html <<!EOF
<html><head><title>index.html</title></head>
<body>Hello /index.html</body>
</html> 
!EOF
    make-files 20 state/site/1K.txt
    make-files 205 state/site/10K.txt
    make-files 512 state/site/25K.txt
    # ../../bin/make-files 2050 state/site/100K.txt
    # ../../bin/make-files 10250 state/site/500K.txt
    # ../../bin/make-files 21000 state/site/1M.txt
    # ../../bin/make-files 210000 state/site/10M.txt
    :
fi

rm -f ./log.txt