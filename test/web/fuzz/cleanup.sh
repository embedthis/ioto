#!/bin/bash
#
#   cleanup.sh - TestMe cleanup script for fuzzing
#

# Archive crashes if found
if [ -d crashes ] && [ "$(ls -A crashes 2>/dev/null)" ]; then
    TIMESTAMP=$(date +%Y%m%d-%H%M%S)
    mkdir -p crashes-archive
    tar -czf crashes-archive/crashes-${TIMESTAMP}.tar.gz crashes/
    echo "✓ Crashes archived to crashes-archive/crashes-${TIMESTAMP}.tar.gz"

    # List unique crashes
    CRASH_COUNT=$(find crashes -type f -name "*.txt" | wc -l | tr -d ' ')
    echo ""
    echo "Crash summary:"
    echo "  Total crash files: $CRASH_COUNT"

    if [ $CRASH_COUNT -gt 0 ]; then
        echo ""
        echo "Crash files by category:"
        for dir in crashes/*/; do
            if [ -d "$dir" ]; then
                count=$(find "$dir" -name "*.txt" | wc -l | tr -d ' ')
                if [ $count -gt 0 ]; then
                    echo "  $(basename "$dir"): $count"
                fi
            fi
        done
    fi
fi

# Cleanup temp files
rm -f fuzz-log.txt
