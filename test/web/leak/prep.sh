#!/usr/bin/env bash
#
#   prep.sh - Prepare leak test environment
#

make-files() {
    COUNT=$1
    OUTPUT=$2
    if [ ! -f $OUTPUT ] ; then
        echo "   [Create] $OUTPUT"
        > $OUTPUT
        while [ $COUNT -gt 0 ] ; do
            echo 0123456789012345678901234567890123456789012345678 >>$OUTPUT
            COUNT=$((COUNT - 1))
        done
        echo END OF DOCUMENT >>$OUTPUT
    fi
}

mkdir -p tmp
mkdir -p site/size

# Create test files of various sizes for leak testing
make-files 20 site/size/1K.txt
make-files 205 site/size/10K.txt
make-files 2050 site/size/100K.txt
make-files 20500 site/size/1M.txt

# Create simple index file
cat > site/index.html <<'EOF'
<!DOCTYPE html>
<html>
<head><title>Leak Test</title></head>
<body><h1>Leak Test Server</h1></body>
</html>
EOF
