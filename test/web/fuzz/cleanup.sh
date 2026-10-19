#!/usr/bin/env bash
#
#   cleanup.sh - TestMe cleanup script for fuzzing
#

echo "Cleaning up fuzz test environment..."

#   Set when any sanitizer report, crash log or core dump is found: the gate fails on it
FAILED=0

# Kill server if still running and not existing
if [ -f ".testme/server.pid" -a ! -f ".testme/existing" ]; then
    SERVER_PID=$(cat .testme/server.pid)
    if [ -n "$SERVER_PID" ] && kill -0 $SERVER_PID 2>/dev/null; then
        echo "Stopping web server (PID $SERVER_PID)..."
        kill $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
        EXIT_CODE=$?
        echo "Server exited with code: $EXIT_CODE"
    else
        echo "Server already stopped"
    fi
    rm -f .testme/server.pid
fi

# Check for server crashes in log
echo "Analyzing web.log for crash indicators..."
if [ -f "web.log" ]; then
    if grep -q "Segmentation fault\|Aborted\|Bus error\|AddressSanitizer\|UndefinedBehaviorSanitizer" web.log; then
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        echo "⚠️  SERVER CRASH DETECTED IN LOG"
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        mkdir -p crashes/server
        cp web.log crashes/server/crash-$(date +%s).log
        echo "Crash log saved to crashes/server/"
        FAILED=1
    fi
fi

# Check crashes/ and the working directory for ASan/UBSan reports.
for kind in asan ubsan ; do
    REPORTS=$(ls -1 crashes/${kind}.* ${kind}.* 2>/dev/null)
    if [ -n "$REPORTS" ]; then
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        echo "⚠️  $(echo $kind | tr a-z A-Z) REPORTS FOUND"
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        mkdir -p crashes/server
        mv $REPORTS crashes/server/ 2>/dev/null || true
        echo "$(echo $kind | tr a-z A-Z) reports saved to crashes/server/"
        FAILED=1
    fi
done

# Check for core dumps
if [ $(ls -1 core.* core 2>/dev/null | wc -l) -gt 0 ]; then
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "⚠️  CORE DUMP FOUND"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    mkdir -p crashes/server
    mv core.* core crashes/server/ 2>/dev/null || true
    echo "Core dump saved to crashes/server/"
    FAILED=1
fi

# Display server crash summary if any server crashes found
if [ -d "crashes/server" ] && [ "$(ls -A crashes/server 2>/dev/null)" ]; then
    echo ""
    echo "═══════════════════════════════════════"
    echo "  SERVER CRASH SUMMARY"
    echo "═══════════════════════════════════════"
    echo ""

    # Show last 50 lines of web.log if tests failed
    if [ "${TESTME_SUCCESS}" != "1" ] && [ -f "web.log" ]; then
        echo "Last 50 lines of web.log:"
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        tail -50 web.log
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    fi
fi

# Archive crashes if found
if [ -d crashes ]; then
    # Count actual crash files (not just empty directories)
    CRASH_COUNT=$(find crashes -type f \( -name "*.txt" -o -name "*.log" -o -name "asan.*" -o -name "ubsan.*" -o -name "core.*" -o -name "core" \) 2>/dev/null | wc -l | tr -d ' ')

    if [ $CRASH_COUNT -gt 0 ]; then
        TIMESTAMP=$(date +%Y%m%d-%H%M%S)
        mkdir -p crashes-archive
        tar -czf crashes-archive/crashes-${TIMESTAMP}.tar.gz crashes/
        echo ""
        echo "✓ Crashes archived to crashes-archive/crashes-${TIMESTAMP}.tar.gz"

        echo ""
        echo "Crash summary:"
        echo "  Total crash files: $CRASH_COUNT"

        echo ""
        echo "Crash files by category:"
        for dir in crashes/*/; do
            if [ -d "$dir" ]; then
                count=$(find "$dir" -type f 2>/dev/null | wc -l | tr -d ' ')
                if [ $count -gt 0 ]; then
                    category=$(basename "$dir")
                    echo "  $category: $count"

                    # Show details for server crashes
                    if [ "$category" = "server" ]; then
                        echo ""
                        echo "  Server crash details:"
                        for crash in "$dir"/*; do
                            if [ -f "$crash" ]; then
                                echo "    - $(basename "$crash")"
                            fi
                        done
                    fi
                fi
            fi
        done
    fi
fi

# Clean up web.log unless tests failed
if [ "${TESTME_SUCCESS}" = "1" ]; then
    # rm -f web.log
    :
else
    # Display web.log on failure (if not already shown above)
    if [ ! -d "crashes/server" ] || [ -z "$(ls -A crashes/server 2>/dev/null)" ]; then
        echo ""
        echo "Web server log (web.log):"
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        cat web.log
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    fi
fi

if [ "$FAILED" != "0" ]; then
    echo "Fuzz gate FAILED: sanitizer reports, crash logs or core dumps were found" >&2
    exit 1
fi
exit 0
