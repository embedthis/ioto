#!/usr/bin/env bash
#
#   get-memory.sh - Get memory usage of a process in KB
#
#   Usage: get-memory.sh <pid>
#

PID=$1

if [ -z "$PID" ]; then
    echo "Usage: $0 <pid>" >&2
    exit 1
fi

# Check if process exists
if ! kill -0 $PID 2>/dev/null; then
    echo "Error: Process $PID does not exist" >&2
    exit 1
fi

# Get memory usage based on platform
case "$(uname -s)" in
    Darwin)
        # macOS: Use ps to get RSS (Resident Set Size) in KB
        # RSS is in 1K blocks on macOS
        MEM=$(ps -o rss= -p $PID 2>/dev/null)
        if [ -z "$MEM" ]; then
            echo "Error: Cannot get memory for process $PID" >&2
            exit 1
        fi
        echo $MEM
        ;;
    Linux)
        # Linux: Use ps to get RSS in KB
        # RSS is in KB on Linux
        MEM=$(ps -o rss= -p $PID 2>/dev/null)
        if [ -z "$MEM" ]; then
            echo "Error: Cannot get memory for process $PID" >&2
            exit 1
        fi
        echo $MEM
        ;;
    *)
        echo "Error: Unsupported platform $(uname -s)" >&2
        exit 1
        ;;
esac
