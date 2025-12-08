#!/usr/bin/env bash
#
#   prep.sh - Prepare benchmark test environment
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

# Create dedicated benchmark directory
mkdir -p site
mkdir -p site/static
mkdir -p site/put
mkdir -p tmp

# Create directory for benchmark results
mkdir -p ../../doc/benchmarks

# Create benchmark-specific test files in static directory
make-files 20 site/static/1K.txt      # 1KB
make-files 205 site/static/10K.txt    # 10KB
make-files 2050 site/static/100K.txt  # 100KB
make-files 20500 site/static/1M.txt   # 1MB

# Create simple index file for basic tests
cat > site/index.html <<'EOF'
<!DOCTYPE html>
<html>
<head><title>Benchmark Test</title></head>
<body><h1>Benchmark Server</h1></body>
</html>
EOF

# Create JSON file for action tests
cat > site/test.json <<'EOF'
{
    "status": "ok",
    "message": "Benchmark test data"
}
EOF

# Create auth directory for authentication tests
mkdir -p site/auth
cat > site/auth/secret.html <<'EOF'
<!DOCTYPE html>
<html>
<head><title>Secret</title></head>
<body><h1>Authenticated Content</h1></body>
</html>
EOF
