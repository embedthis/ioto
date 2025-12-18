#!/bin/bash
#
#   cleanup.sh - Cleanup benchmark test environment
#

# Kill any web servers on benchmark ports (4260, 4261)
# lsof -ti:4260 2>/dev/null | xargs kill -9 2>/dev/null || true
# lsof -ti:4261 2>/dev/null | xargs kill -9 2>/dev/null || true

# Clean up pid file
rm -f bench.pid

# Clean up debug logs
rm -f bench-auth-debug.log

# Clean up put benchmark directory
rm -rf site/put/* 2>/dev/null || true

# Clean up multipart upload files in tmp directory (use find to handle large counts)
find tmp -name 'bench-mp-*.txt' -delete 2>/dev/null || true

# Note: Keep web.log for debugging if tests fail
# TestMe will handle log preservation based on test results
rm -f web.log

rm -f tmp/*
