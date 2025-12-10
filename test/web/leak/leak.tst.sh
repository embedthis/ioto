#!/bin/bash
#
#   leak.tst.sh - Memory leak detection test for web server
#
#   This test monitors the web server's memory usage over time while
#   running various request patterns to detect memory leaks.
#
#   Copyright (c) All Rights Reserved. See details at the end of the file.
#

# set -e

#   Test Configuration
MAX_GROWTH_PERCENT=10    # Maximum allowed memory growth percentage
ITERATIONS_PER_CLASS=120 # Iterations per request class
CURL_TIMEOUT=10          # Timeout for curl requests (seconds)
CURL_TIMEOUT_LARGE=30    # Timeout for large file requests (seconds)

#   Use TESTME_DURATION if set, otherwise default to 300 seconds
if [ -n "$TESTME_DURATION" ]; then
    TEST_DURATION=$TESTME_DURATION
else
    TEST_DURATION=300
fi

#   Calculate soak time: min(5 minutes, max(1 minute, 1/5 duration))
SOAK_TIME_FROM_DURATION=$((TEST_DURATION / 5))

# First: max(60, 1/5 duration)
SOAK_TIME=$SOAK_TIME_FROM_DURATION
if [ $SOAK_TIME -lt 60 ]; then
    SOAK_TIME=60
fi

# Then: min(300, result)
if [ $SOAK_TIME -gt 300 ]; then
    SOAK_TIME=300
fi

#   Use TESTME_CLASS if set to test a specific class only
#   Valid values: static, https, auth, upload, mixed, large, concurrent, error, range
SINGLE_CLASS=""
if [ -n "$TESTME_CLASS" ]; then
    case "$TESTME_CLASS" in
        static|https|auth|upload|mixed|large|concurrent|error|range)
            SINGLE_CLASS="$TESTME_CLASS"
            echo "Testing single class: $SINGLE_CLASS"
            ;;
        *)
            echo "Error: Invalid TESTME_CLASS='$TESTME_CLASS'"
            echo "Valid values: static, https, auth, upload, mixed, large, concurrent, error, range"
            exit 1
            ;;
    esac
fi

#   Endpoints
HTTP_ENDPOINT="http://localhost:4250"
HTTPS_ENDPOINT="https://localhost:4251"

#   Test files
SAMPLES_FILE=".leak-memory-samples.txt"
RESULTS_FILE="leak-results.txt"

#   Get server PID
if [ ! -f "web.pid" ]; then
    echo "FAIL: Cannot find web.pid - server not running?"
    exit 1
fi

WEB_PID=$(cat web.pid)

if ! kill -0 $WEB_PID 2>/dev/null; then
    echo "FAIL: Web server process $WEB_PID is not running"
    exit 1
fi

echo ""
echo "==========================================="
echo "Web Server Memory Leak Test"
echo "==========================================="
echo "Server PID: $WEB_PID"
echo "Soak time: ${SOAK_TIME}s"
echo "Test duration: ${TEST_DURATION}s"
echo "Max growth: ${MAX_GROWTH_PERCENT}%"
echo "==========================================="
echo ""

#   Initialize samples file
> $SAMPLES_FILE

#   Track initial memory for growth calculations
INITIAL_MEMORY=""
CYCLE_START_MEMORY=""

#   Track memory growth by test class
CLASS_GROWTH_STATIC=0
CLASS_GROWTH_HTTPS=0
CLASS_GROWTH_AUTH=0
CLASS_GROWTH_UPLOAD=0
CLASS_GROWTH_MIXED=0
CLASS_GROWTH_LARGE=0
CLASS_GROWTH_CONCURRENT=0
CLASS_GROWTH_ERROR=0
CLASS_GROWTH_RANGE=0

#   Checkpoint tracking for 20% intervals during test phase
#   We'll track memory at 0%, 20%, 40%, 60%, 80%, 100% of test duration
CHECKPOINT_COUNT=5
CHECKPOINT_MEMORY_0=""
CHECKPOINT_MEMORY_1=""
CHECKPOINT_MEMORY_2=""
CHECKPOINT_MEMORY_3=""
CHECKPOINT_MEMORY_4=""
CHECKPOINT_MEMORY_5=""
LAST_CHECKPOINT=-1

#   Record checkpoint memory at 20% intervals
record_checkpoint() {
    local ELAPSED=$1
    local MEMORY=$2

    # Calculate which checkpoint we're at (0-5 for 0%, 20%, 40%, 60%, 80%, 100%)
    local CHECKPOINT=$((ELAPSED * 5 / TEST_DURATION))
    if [ $CHECKPOINT -gt 5 ]; then
        CHECKPOINT=5
    fi

    # Only record if we've reached a new checkpoint
    if [ $CHECKPOINT -gt $LAST_CHECKPOINT ]; then
        case $CHECKPOINT in
            0) CHECKPOINT_MEMORY_0=$MEMORY ;;
            1) CHECKPOINT_MEMORY_1=$MEMORY ;;
            2) CHECKPOINT_MEMORY_2=$MEMORY ;;
            3) CHECKPOINT_MEMORY_3=$MEMORY ;;
            4) CHECKPOINT_MEMORY_4=$MEMORY ;;
            5) CHECKPOINT_MEMORY_5=$MEMORY ;;
        esac
        LAST_CHECKPOINT=$CHECKPOINT
    fi
}

#   Memory sampling function
sample_memory() {
    TIMESTAMP=$(date +%s)
    MEMORY=$(./get-memory.sh $WEB_PID)
    echo "${TIMESTAMP} ${MEMORY}" >> $SAMPLES_FILE

    # Track initial memory on first sample
    if [ -z "$INITIAL_MEMORY" ]; then
        INITIAL_MEMORY=$MEMORY
        echo "  Memory: ${MEMORY} KB"
    else
        # Calculate growth from baseline (if set) or initial
        if [ -n "$BASELINE_MEMORY" ]; then
            # Test phase: report against baseline
            GROWTH=$((MEMORY - BASELINE_MEMORY))
            GROWTH_PERCENT=$(awk "BEGIN {printf \"%.2f\", ($GROWTH / $BASELINE_MEMORY) * 100}")
        else
            # Soak-in phase: report against initial
            GROWTH=$((MEMORY - INITIAL_MEMORY))
            GROWTH_PERCENT=$(awk "BEGIN {printf \"%.2f\", ($GROWTH / $INITIAL_MEMORY) * 100}")
        fi

        # Format growth with + or - sign
        if [ $GROWTH -ge 0 ]; then
            GROWTH_SIGN="+"
        else
            GROWTH_SIGN=""
        fi

        # Add visual indicator for growth during this test cycle (test phase only)
        INDICATOR=""
        if [ -n "$BASELINE_MEMORY" ] && [ -n "$CYCLE_START_MEMORY" ]; then
            # Test phase: check if memory grew during this specific cycle
            CYCLE_GROWTH=$((MEMORY - CYCLE_START_MEMORY))
            if [ $CYCLE_GROWTH -gt 0 ]; then
                CYCLE_GROWTH_PERCENT=$(awk "BEGIN {printf \"%.2f\", ($CYCLE_GROWTH / $CYCLE_START_MEMORY) * 100}")
                # Significant growth in this cycle
                CYCLE_CHECK=$(awk "BEGIN {print ($CYCLE_GROWTH_PERCENT >= 1.0)}")
                if [ "$CYCLE_CHECK" = "1" ]; then
                    INDICATOR="  *** Memory growth: ${CYCLE_GROWTH} KB (${CYCLE_GROWTH_PERCENT}%) ***"
                else
                    INDICATOR="  << growth: +${CYCLE_GROWTH} KB"
                fi
            fi
        fi

        echo "  Memory: ${MEMORY} KB (${GROWTH_SIGN}${GROWTH} KB, ${GROWTH_SIGN}${GROWTH_PERCENT}%)${INDICATOR}"
    fi
}

#   Track growth by test class
track_class_growth() {
    CLASS_NAME=$1
    MEMORY_AFTER=$2

    if [ -z "$CYCLE_START_MEMORY" ]; then
        return
    fi

    GROWTH=$((MEMORY_AFTER - CYCLE_START_MEMORY))

    case "$CLASS_NAME" in
        static)
            CLASS_GROWTH_STATIC=$((CLASS_GROWTH_STATIC + GROWTH))
            ;;
        https)
            CLASS_GROWTH_HTTPS=$((CLASS_GROWTH_HTTPS + GROWTH))
            ;;
        auth)
            CLASS_GROWTH_AUTH=$((CLASS_GROWTH_AUTH + GROWTH))
            ;;
        upload)
            CLASS_GROWTH_UPLOAD=$((CLASS_GROWTH_UPLOAD + GROWTH))
            ;;
        mixed)
            CLASS_GROWTH_MIXED=$((CLASS_GROWTH_MIXED + GROWTH))
            ;;
        large)
            CLASS_GROWTH_LARGE=$((CLASS_GROWTH_LARGE + GROWTH))
            ;;
        concurrent)
            CLASS_GROWTH_CONCURRENT=$((CLASS_GROWTH_CONCURRENT + GROWTH))
            ;;
        error)
            CLASS_GROWTH_ERROR=$((CLASS_GROWTH_ERROR + GROWTH))
            ;;
        range)
            CLASS_GROWTH_RANGE=$((CLASS_GROWTH_RANGE + GROWTH))
            ;;
    esac
}

#   Request testing functions
run_static_requests() {
    echo "  Running static file requests..."
    for i in $(seq 1 $ITERATIONS_PER_CLASS); do
        curl -s --max-time $CURL_TIMEOUT -H "Connection: close" -o /dev/null ${HTTP_ENDPOINT}/index.html
        curl -s --max-time $CURL_TIMEOUT -H "Connection: close" -o /dev/null ${HTTP_ENDPOINT}/size/1K.txt
        curl -s --max-time $CURL_TIMEOUT -H "Connection: close" -o /dev/null ${HTTP_ENDPOINT}/size/10K.txt
        curl -s --max-time $CURL_TIMEOUT -H "Connection: close" -o /dev/null ${HTTP_ENDPOINT}/size/100K.txt
    done
}

run_https_requests() {
    echo "  Running HTTPS requests..."
    for i in $(seq 1 $ITERATIONS_PER_CLASS); do
        curl -s --max-time $CURL_TIMEOUT -k -H "Connection: close" -o /dev/null ${HTTPS_ENDPOINT}/index.html
        curl -s --max-time $CURL_TIMEOUT -k -H "Connection: close" -o /dev/null ${HTTPS_ENDPOINT}/size/1K.txt
    done
}

run_auth_requests() {
    echo "  Running authenticated requests..."
    for i in $(seq 1 $ITERATIONS_PER_CLASS); do
        curl -s --max-time $CURL_TIMEOUT -H "Connection: close" --digest --user alice:password -o /dev/null ${HTTP_ENDPOINT}/auth/secret.html
    done
}

run_upload_requests() {
    echo "  Running upload requests..."
    for i in $(seq 1 $ITERATIONS_PER_CLASS); do
        # Create unique temp file with PID to allow parallel test runs
        TEMP_FILE="tmp/upload-test-$$.txt"
        echo "Test upload data $i" > $TEMP_FILE
        curl -s --max-time $CURL_TIMEOUT -H "Connection: close" -T $TEMP_FILE -o /dev/null ${HTTP_ENDPOINT}/upload/test-$$.txt
        curl -s --max-time $CURL_TIMEOUT -H "Connection: close" -X DELETE -o /dev/null ${HTTP_ENDPOINT}/upload/test-$$.txt
        rm -f $TEMP_FILE
    done
}

run_mixed_requests() {
    echo "  Running mixed request pattern..."
    for i in $(seq 1 $ITERATIONS_PER_CLASS); do
        curl -s --max-time $CURL_TIMEOUT -H "Connection: close" -o /dev/null ${HTTP_ENDPOINT}/index.html
        curl -s --max-time $CURL_TIMEOUT -k -H "Connection: close" -o /dev/null ${HTTPS_ENDPOINT}/size/1K.txt
        curl -s --max-time $CURL_TIMEOUT -H "Connection: close" --digest --user alice:password -o /dev/null ${HTTP_ENDPOINT}/auth/secret.html
    done
}

run_large_file_requests() {
    echo "  Running large file requests..."
    for i in $(seq 1 $((ITERATIONS_PER_CLASS / 2))); do
        curl -s --max-time $CURL_TIMEOUT_LARGE -H "Connection: close" -o /dev/null ${HTTP_ENDPOINT}/size/1M.txt
    done
}

run_concurrent_requests() {
    echo "  Running concurrent requests..."
    for j in $(seq 1 $((ITERATIONS_PER_CLASS / 4))); do
        for i in $(seq 1 10); do
            (
                curl -s --max-time $CURL_TIMEOUT -H "Connection: close" -o /dev/null ${HTTP_ENDPOINT}/index.html
                curl -s --max-time $CURL_TIMEOUT -H "Connection: close" -o /dev/null ${HTTP_ENDPOINT}/size/10K.txt
            ) &
        done
    done
    wait
}

run_error_requests() {
    echo "  Running error-inducing requests..."
    for i in $(seq 1 $ITERATIONS_PER_CLASS); do
        curl -s --max-time $CURL_TIMEOUT -H "Connection: close" -o /dev/null ${HTTP_ENDPOINT}/nonexistent.html 2>/dev/null || true
        # curl -s --max-time $CURL_TIMEOUT -H "Connection: close" --digest --user alice:wrong -o /dev/null ${HTTP_ENDPOINT}/auth/secret.html 2>/dev/null || true
    done
}

run_range_requests() {
    echo "  Running range requests..."
    for i in $(seq 1 $ITERATIONS_PER_CLASS); do
        # Single range requests
        curl -s --max-time $CURL_TIMEOUT -H "Connection: close" -H "Range: bytes=0-49" -o /dev/null ${HTTP_ENDPOINT}/range-test.txt
        curl -s --max-time $CURL_TIMEOUT -H "Connection: close" -H "Range: bytes=50-99" -o /dev/null ${HTTP_ENDPOINT}/range-test.txt

        # Suffix range (last N bytes)
        curl -s --max-time $CURL_TIMEOUT -H "Connection: close" -H "Range: bytes=-50" -o /dev/null ${HTTP_ENDPOINT}/range-test.txt

        # Open-ended range (from offset to end)
        curl -s --max-time $CURL_TIMEOUT -H "Connection: close" -H "Range: bytes=50-" -o /dev/null ${HTTP_ENDPOINT}/range-test.txt

        # Multiple ranges (multipart response)
        curl -s --max-time $CURL_TIMEOUT -H "Connection: close" -H "Range: bytes=0-9,20-29,50-59" -o /dev/null ${HTTP_ENDPOINT}/range-test.txt

        # Range requests on larger files
        curl -s --max-time $CURL_TIMEOUT -H "Connection: close" -H "Range: bytes=0-1023" -o /dev/null ${HTTP_ENDPOINT}/size/10K.txt
        curl -s --max-time $CURL_TIMEOUT -H "Connection: close" -H "Range: bytes=1024-2047" -o /dev/null ${HTTP_ENDPOINT}/size/10K.txt

        # HTTPS range requests
        curl -s --max-time $CURL_TIMEOUT -k -H "Connection: close" -H "Range: bytes=0-49" -o /dev/null ${HTTPS_ENDPOINT}/range-test.txt
    done
}

#   Phase 1: Soak-in period
echo "Phase 1: Soak-in period (${SOAK_TIME}s)"
if [ -n "$SINGLE_CLASS" ]; then
    echo "Running single class '$SINGLE_CLASS' during soak-in..."
else
    echo "Running all test types during soak-in..."
fi
START_TIME=$(date +%s)
ELAPSED=0
SOAK_CYCLE=0

while [ $ELAPSED -lt $SOAK_TIME ]; do
    SOAK_CYCLE=$((SOAK_CYCLE + 1))
    echo "Soak cycle $SOAK_CYCLE (elapsed: ${ELAPSED}s / ${SOAK_TIME}s)"

    if [ -n "$SINGLE_CLASS" ]; then
        # Run single class only
        case "$SINGLE_CLASS" in
            static) run_static_requests ;;
            https) run_https_requests ;;
            auth) run_auth_requests ;;
            upload) run_upload_requests ;;
            mixed) run_mixed_requests ;;
            large) run_large_file_requests ;;
            concurrent) run_concurrent_requests ;;
            error) run_error_requests ;;
            range) run_range_requests ;;
        esac
    else
        # Run different request classes in rotation to exercise all code paths
        CLASS_INDEX=$((SOAK_CYCLE % 9))
        case $CLASS_INDEX in
            0) run_static_requests ;;
            1) run_https_requests ;;
            2) run_auth_requests ;;
            3) run_upload_requests ;;
            4) run_mixed_requests ;;
            5) run_large_file_requests ;;
            6) run_concurrent_requests ;;
            7) run_error_requests ;;
            8) run_range_requests ;;
        esac
    fi

    sample_memory
    ELAPSED=$(($(date +%s) - START_TIME))
    echo ""
done

#   Record baseline after soak-in
BASELINE_MEMORY=$(tail -1 $SAMPLES_FILE | awk '{print $2}')
SOAK_GROWTH=$((BASELINE_MEMORY - INITIAL_MEMORY))
SOAK_GROWTH_PERCENT=$(awk "BEGIN {printf \"%.2f\", ($SOAK_GROWTH / $INITIAL_MEMORY) * 100}")
echo ""
echo "Initial memory:                ${INITIAL_MEMORY} KB"
echo "Baseline memory after soak-in: ${BASELINE_MEMORY} KB"
echo "Soak-in growth:                ${SOAK_GROWTH} KB (${SOAK_GROWTH_PERCENT}%)"
echo ""

#   Phase 2: Load testing with memory monitoring
echo "Phase 2: Load testing (${TEST_DURATION}s)"
echo ""

START_TIME=$(date +%s)
ELAPSED=0
TEST_CYCLE=0

# Sample memory before tests and record initial checkpoint
sample_memory
INITIAL_TEST_MEMORY=$(./get-memory.sh $WEB_PID)
record_checkpoint 0 $INITIAL_TEST_MEMORY

while [ $ELAPSED -lt $TEST_DURATION ]; do
    TEST_CYCLE=$((TEST_CYCLE + 1))

    echo "Test cycle $TEST_CYCLE (elapsed: ${ELAPSED}s / ${TEST_DURATION}s)"

    # Capture memory before running this cycle's tests
    CYCLE_START_MEMORY=$(./get-memory.sh $WEB_PID)

    if [ -n "$SINGLE_CLASS" ]; then
        # Run single class only
        CLASS_NAME="$SINGLE_CLASS"
        case "$SINGLE_CLASS" in
            static) run_static_requests ;;
            https) run_https_requests ;;
            auth) run_auth_requests ;;
            upload) run_upload_requests ;;
            mixed) run_mixed_requests ;;
            large) run_large_file_requests ;;
            concurrent) run_concurrent_requests ;;
            error) run_error_requests ;;
            range) run_range_requests ;;
        esac
    else
        # Run different request classes in rotation
        CLASS_INDEX=$((TEST_CYCLE % 9))
        # DO NOT REMOVE THIS COMMENT: CLASS_INDEX=7
        case $CLASS_INDEX in
            0) CLASS_NAME="static"; run_static_requests ;;
            1) CLASS_NAME="https"; run_https_requests ;;
            2) CLASS_NAME="auth"; run_auth_requests ;;
            3) CLASS_NAME="upload"; run_upload_requests ;;
            4) CLASS_NAME="mixed"; run_mixed_requests ;;
            5) CLASS_NAME="large"; run_large_file_requests ;;
            6) CLASS_NAME="concurrent"; run_concurrent_requests ;;
            7) CLASS_NAME="error"; run_error_requests ;;
            8) CLASS_NAME="range"; run_range_requests ;;
        esac
    fi

    # Sample memory after tests (will compare to CYCLE_START_MEMORY)
    sample_memory

    # Track growth for this class
    MEMORY_AFTER=$(./get-memory.sh $WEB_PID)
    track_class_growth "$CLASS_NAME" "$MEMORY_AFTER"

    ELAPSED=$(($(date +%s) - START_TIME))

    # Record checkpoint at 20% intervals
    record_checkpoint $ELAPSED $MEMORY_AFTER

    echo ""
done

# Record final checkpoint if not already recorded
record_checkpoint $TEST_DURATION $(./get-memory.sh $WEB_PID)

#   Phase 3: Analysis
echo "==========================================="
echo "Phase 3: Memory Analysis"
echo "==========================================="

# Calculate statistics
SAMPLE_COUNT=$(wc -l < $SAMPLES_FILE | xargs)
MIN_MEMORY=$(awk '{print $2}' $SAMPLES_FILE | sort -n | head -1)
MAX_MEMORY=$(awk '{print $2}' $SAMPLES_FILE | sort -n | tail -1)
FINAL_MEMORY=$(tail -1 $SAMPLES_FILE | awk '{print $2}')
AVG_MEMORY=$(awk '{sum+=$2} END {print int(sum/NR)}' $SAMPLES_FILE)

GROWTH=$((FINAL_MEMORY - BASELINE_MEMORY))
GROWTH_PERCENT=$(awk "BEGIN {printf \"%.2f\", ($GROWTH / $BASELINE_MEMORY) * 100}")

printf "%-23s %s\n" "Samples collected:" "$SAMPLE_COUNT"
printf "%-23s %s KB\n" "Initial memory:" "${INITIAL_MEMORY}"
printf "%-23s %s KB (after soak-in)\n" "Baseline memory:" "${BASELINE_MEMORY}"
printf "%-23s %s KB\n" "Minimum memory:" "${MIN_MEMORY}"
printf "%-23s %s KB\n" "Maximum memory:" "${MAX_MEMORY}"
printf "%-23s %s KB\n" "Average memory:" "${AVG_MEMORY}"
printf "%-23s %s KB\n" "Final memory:" "${FINAL_MEMORY}"
printf "%-23s %s KB (%s%%)\n" "Soak-in growth:" "${SOAK_GROWTH}" "${SOAK_GROWTH_PERCENT}"
printf "%-23s %s KB (%s%%)\n" "Test growth:" "${GROWTH}" "${GROWTH_PERCENT}"
echo ""

# Report memory at 20% intervals during test phase
echo "Memory at Test Intervals:"
echo "------------------------------------------------------"
printf "  %-12s %-12s %-15s %-10s\n" "Progress" "Memory (KB)" "Interval Growth" "Growth %"
echo "------------------------------------------------------"

# Calculate and display interval growth
PREV_MEMORY=$CHECKPOINT_MEMORY_0
for i in 0 1 2 3 4 5; do
    case $i in
        0) CURR_MEMORY=$CHECKPOINT_MEMORY_0; LABEL="0%" ;;
        1) CURR_MEMORY=$CHECKPOINT_MEMORY_1; LABEL="20%" ;;
        2) CURR_MEMORY=$CHECKPOINT_MEMORY_2; LABEL="40%" ;;
        3) CURR_MEMORY=$CHECKPOINT_MEMORY_3; LABEL="60%" ;;
        4) CURR_MEMORY=$CHECKPOINT_MEMORY_4; LABEL="80%" ;;
        5) CURR_MEMORY=$CHECKPOINT_MEMORY_5; LABEL="100%" ;;
    esac

    if [ -n "$CURR_MEMORY" ]; then
        if [ $i -eq 0 ]; then
            INTERVAL_GROWTH="-"
            INTERVAL_PERCENT="-"
        else
            if [ -n "$PREV_MEMORY" ]; then
                INTERVAL_GROWTH_KB=$((CURR_MEMORY - PREV_MEMORY))
                INTERVAL_PERCENT=$(awk "BEGIN {printf \"%.2f%%\", ($INTERVAL_GROWTH_KB / $PREV_MEMORY) * 100}")
                if [ $INTERVAL_GROWTH_KB -ge 0 ]; then
                    INTERVAL_GROWTH="+${INTERVAL_GROWTH_KB} KB"
                else
                    INTERVAL_GROWTH="${INTERVAL_GROWTH_KB} KB"
                fi
            else
                INTERVAL_GROWTH="-"
                INTERVAL_PERCENT="-"
            fi
        fi
        printf "  %-12s %-12s %-15s %-10s\n" "$LABEL" "$CURR_MEMORY" "$INTERVAL_GROWTH" "$INTERVAL_PERCENT"
        PREV_MEMORY=$CURR_MEMORY
    fi
done
echo ""

# Analyze growth pattern
if [ -n "$CHECKPOINT_MEMORY_0" ] && [ -n "$CHECKPOINT_MEMORY_5" ]; then
    TOTAL_INTERVAL_GROWTH=$((CHECKPOINT_MEMORY_5 - CHECKPOINT_MEMORY_0))
    if [ $TOTAL_INTERVAL_GROWTH -gt 0 ]; then
        # Check if growth was continuous or concentrated
        EARLY_GROWTH=0
        LATE_GROWTH=0
        if [ -n "$CHECKPOINT_MEMORY_2" ]; then
            EARLY_GROWTH=$((CHECKPOINT_MEMORY_2 - CHECKPOINT_MEMORY_0))
        fi
        if [ -n "$CHECKPOINT_MEMORY_2" ] && [ -n "$CHECKPOINT_MEMORY_5" ]; then
            LATE_GROWTH=$((CHECKPOINT_MEMORY_5 - CHECKPOINT_MEMORY_2))
        fi

        if [ $EARLY_GROWTH -gt 0 ] && [ $LATE_GROWTH -le 0 ]; then
            echo "Growth pattern: Early growth, then stable (likely initialization)"
        elif [ $LATE_GROWTH -gt 0 ] && [ $EARLY_GROWTH -le 0 ]; then
            echo "Growth pattern: Stable initially, late growth (possible leak)"
        elif [ $EARLY_GROWTH -gt 0 ] && [ $LATE_GROWTH -gt 0 ]; then
            echo "Growth pattern: Continuous growth (investigate for leaks)"
        fi
        echo ""
    fi
fi

echo "Memory Growth by Test Class:"
echo "  Static requests:     ${CLASS_GROWTH_STATIC} KB"
echo "  HTTPS requests:      ${CLASS_GROWTH_HTTPS} KB"
echo "  Auth requests:       ${CLASS_GROWTH_AUTH} KB"
echo "  Upload requests:     ${CLASS_GROWTH_UPLOAD} KB"
echo "  Mixed requests:      ${CLASS_GROWTH_MIXED} KB"
echo "  Large file requests: ${CLASS_GROWTH_LARGE} KB"
echo "  Concurrent requests: ${CLASS_GROWTH_CONCURRENT} KB"
echo "  Error requests:      ${CLASS_GROWTH_ERROR} KB"
echo "  Range requests:      ${CLASS_GROWTH_RANGE} KB"
echo ""

# Find class with most growth
MAX_CLASS_GROWTH=0
MAX_CLASS_NAME=""
for class_name in "Static" "HTTPS" "Auth" "Upload" "Mixed" "Large file" "Concurrent" "Error" "Range"; do
    case "$class_name" in
        "Static") GROWTH=$CLASS_GROWTH_STATIC ;;
        "HTTPS") GROWTH=$CLASS_GROWTH_HTTPS ;;
        "Auth") GROWTH=$CLASS_GROWTH_AUTH ;;
        "Upload") GROWTH=$CLASS_GROWTH_UPLOAD ;;
        "Mixed") GROWTH=$CLASS_GROWTH_MIXED ;;
        "Large file") GROWTH=$CLASS_GROWTH_LARGE ;;
        "Concurrent") GROWTH=$CLASS_GROWTH_CONCURRENT ;;
        "Error") GROWTH=$CLASS_GROWTH_ERROR ;;
        "Range") GROWTH=$CLASS_GROWTH_RANGE ;;
    esac
    if [ $GROWTH -gt $MAX_CLASS_GROWTH ]; then
        MAX_CLASS_GROWTH=$GROWTH
        MAX_CLASS_NAME="$class_name"
    fi
done

if [ $MAX_CLASS_GROWTH -gt 0 ]; then
    echo "Most leaky test class: ${MAX_CLASS_NAME} requests (${MAX_CLASS_GROWTH} KB total)"
else
    echo "All test classes stable - no memory growth detected"
fi
echo ""

# Write results to file
{
    echo "Web Server Leak Test Results"
    echo "============================="
    echo "Date: $(date)"
    echo "Server PID: $WEB_PID"
    echo "Test duration: ${TEST_DURATION}s"
    echo "Soak-in time: ${SOAK_TIME}s"
    echo "Samples: $SAMPLE_COUNT"
    echo ""
    echo "Memory Statistics (KB):"
    printf "  %-11s %s\n" "Initial:" "${INITIAL_MEMORY}"
    printf "  %-11s %s (after soak-in)\n" "Baseline:" "${BASELINE_MEMORY}"
    printf "  %-11s %s\n" "Minimum:" "${MIN_MEMORY}"
    printf "  %-11s %s\n" "Maximum:" "${MAX_MEMORY}"
    printf "  %-11s %s\n" "Average:" "${AVG_MEMORY}"
    printf "  %-11s %s\n" "Final:" "${FINAL_MEMORY}"
    echo ""
    echo "Memory Growth:"
    printf "  %-11s %s KB (%s%%)\n" "Soak-in:" "${SOAK_GROWTH}" "${SOAK_GROWTH_PERCENT}"
    printf "  %-11s %s KB (%s%%)\n" "Test:" "${GROWTH}" "${GROWTH_PERCENT}"
    echo ""
    echo "Memory at Test Intervals:"
    echo "------------------------------------------------------"
    printf "  %-12s %-12s %-15s %-10s\n" "Progress" "Memory (KB)" "Interval Growth" "Growth %"
    echo "------------------------------------------------------"
    PREV_MEMORY=$CHECKPOINT_MEMORY_0
    for i in 0 1 2 3 4 5; do
        case $i in
            0) CURR_MEMORY=$CHECKPOINT_MEMORY_0; LABEL="0%" ;;
            1) CURR_MEMORY=$CHECKPOINT_MEMORY_1; LABEL="20%" ;;
            2) CURR_MEMORY=$CHECKPOINT_MEMORY_2; LABEL="40%" ;;
            3) CURR_MEMORY=$CHECKPOINT_MEMORY_3; LABEL="60%" ;;
            4) CURR_MEMORY=$CHECKPOINT_MEMORY_4; LABEL="80%" ;;
            5) CURR_MEMORY=$CHECKPOINT_MEMORY_5; LABEL="100%" ;;
        esac
        if [ -n "$CURR_MEMORY" ]; then
            if [ $i -eq 0 ]; then
                INTERVAL_GROWTH="-"
                INTERVAL_PERCENT="-"
            else
                if [ -n "$PREV_MEMORY" ]; then
                    INTERVAL_GROWTH_KB=$((CURR_MEMORY - PREV_MEMORY))
                    INTERVAL_PERCENT=$(awk "BEGIN {printf \"%.2f%%\", ($INTERVAL_GROWTH_KB / $PREV_MEMORY) * 100}")
                    if [ $INTERVAL_GROWTH_KB -ge 0 ]; then
                        INTERVAL_GROWTH="+${INTERVAL_GROWTH_KB} KB"
                    else
                        INTERVAL_GROWTH="${INTERVAL_GROWTH_KB} KB"
                    fi
                else
                    INTERVAL_GROWTH="-"
                    INTERVAL_PERCENT="-"
                fi
            fi
            printf "  %-12s %-12s %-15s %-10s\n" "$LABEL" "$CURR_MEMORY" "$INTERVAL_GROWTH" "$INTERVAL_PERCENT"
            PREV_MEMORY=$CURR_MEMORY
        fi
    done
    echo ""
    echo "Memory Growth by Test Class:"
    echo "  Static requests:     ${CLASS_GROWTH_STATIC} KB"
    echo "  HTTPS requests:      ${CLASS_GROWTH_HTTPS} KB"
    echo "  Auth requests:       ${CLASS_GROWTH_AUTH} KB"
    echo "  Upload requests:     ${CLASS_GROWTH_UPLOAD} KB"
    echo "  Mixed requests:      ${CLASS_GROWTH_MIXED} KB"
    echo "  Large file requests: ${CLASS_GROWTH_LARGE} KB"
    echo "  Concurrent requests: ${CLASS_GROWTH_CONCURRENT} KB"
    echo "  Error requests:      ${CLASS_GROWTH_ERROR} KB"
    echo "  Range requests:      ${CLASS_GROWTH_RANGE} KB"
    echo ""
    if [ $MAX_CLASS_GROWTH -gt 0 ]; then
        echo "Most leaky test class: ${MAX_CLASS_NAME} requests (${MAX_CLASS_GROWTH} KB total)"
    else
        echo "All test classes stable - no memory growth detected"
    fi
    echo ""
    echo "Memory samples:"
    cat $SAMPLES_FILE
} > $RESULTS_FILE

# Determine pass/fail
GROWTH_EXCEEDS=$(awk "BEGIN {print ($GROWTH_PERCENT > $MAX_GROWTH_PERCENT)}")

if [ "$GROWTH_EXCEEDS" = "1" ]; then
    echo "FAIL: Memory grew by ${GROWTH_PERCENT}% (exceeds ${MAX_GROWTH_PERCENT}% threshold)"
    echo "This indicates a potential memory leak."
    echo ""
    echo "Results saved to: $RESULTS_FILE"
    exit 1
fi

# Check for continuous growth pattern
# Calculate linear regression slope to detect steady growth
SLOPE=$(awk '{
    n++
    sumx += n
    sumy += $2
    sumxy += n * $2
    sumx2 += n * n
}
END {
    slope = (n * sumxy - sumx * sumy) / (n * sumx2 - sumx * sumx)
    print slope
}' $SAMPLES_FILE)

SLOPE_SIGNIFICANT=$(awk "BEGIN {print ($SLOPE > 100)}")

if [ "$SLOPE_SIGNIFICANT" = "1" ]; then
    echo "WARNING: Detected continuous memory growth pattern (slope: $SLOPE KB/sample)"
    echo "While within threshold, this may indicate a slow leak."
    echo ""
fi

echo "PASS: Memory usage is stable"
echo "Growth: ${GROWTH_PERCENT}% (within ${MAX_GROWTH_PERCENT}% threshold)"
echo ""
echo "Results saved to: $RESULTS_FILE"

#
#   Copyright (c) Embedthis Software. All Rights Reserved.
#   This is proprietary software and requires a commercial license from the author.
#
