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

mkdir -p site/size certs

if [ ! -f certs/test.crt ] ; then
    cp ../../certs/*.crt certs
    cp ../../certs/*.key certs
fi

if [ ! -f site/size/1K.txt ] ; then
    make-files 20 site/size/1K.txt
    make-files 205 site/size/10K.txt
    make-files 512 site/size/25K.txt
    # ../../bin/make-files 2050 site/size/100K.txt
    # ../../bin/make-files 10250 site/size/500K.txt
    # ../../bin/make-files 21000 site/size/1M.txt
    # ../../bin/make-files 210000 site/size/10M.txt
    cat >site/index.html <<!EOF
<html><head><title>index.html</title></head>
<body>Hello /index.html</body>
</html> 
!EOF
fi

killall web

rm -f ./log.txt