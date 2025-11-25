/*
    bench-utils.h - Benchmark utility functions and data structures

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#ifndef _h_BENCH_UTILS
#define _h_BENCH_UTILS 1

/********************************** Includes **********************************/

#include "r.h"

#ifdef __cplusplus
extern "C" {
#endif

/*********************************** Types ************************************/

/**
 * Benchmark result structure
 * Stores timing and statistical data for a benchmark run
 */
typedef struct BenchResult {
    char *name;               // Benchmark name
    int iterations;           // Number of iterations run
    Ticks totalTime;          // Total time (milliseconds)
    Ticks minTime;            // Minimum latency (ms)
    Ticks maxTime;            // Maximum latency (ms)
    double avgTime;           // Average latency (ms)
    double p95Time;           // 95th percentile (ms)
    double p99Time;           // 99th percentile (ms)
    double requestsPerSec;    // Throughput
    int64 bytesTransferred;   // Total bytes
    int errors;               // Error count
    RList *samples;           // Individual timing samples for percentiles
} BenchResult;

/********************************** Prototypes ********************************/

/**
 * Configure duration-based benchmarking from TESTME_DURATION environment variable
 * Divides total duration into soak (10%) and benchmark (90%) phases
 * Benchmark time is divided equally among test groups
 * @param numGroups Number of test groups to allocate benchmark time among
 */
extern void configureDuration(int numGroups);

/**
 * Get soak phase duration
 * @return Soak duration in milliseconds
 */
extern Ticks getSoakDuration(void);

/**
 * Get benchmark duration per group
 * @return Duration in milliseconds allocated to each test group
 */
extern Ticks getBenchDuration(void);

/**
 * Create a new benchmark result structure
 * @param name Benchmark name
 * @return Allocated benchmark result structure
 */
extern BenchResult *createBenchResult(cchar *name);

/**
 * Free a benchmark result structure
 * @param result Benchmark result to free
 */
extern void freeBenchResult(BenchResult *result);

/**
 * Record a timing sample
 * @param result Benchmark result structure
 * @param elapsed Elapsed time in milliseconds
 */
extern void recordTiming(BenchResult *result, Ticks elapsed);

/**
 * Calculate statistics from recorded samples
 * Computes min, max, avg, p95, p99, and requests/sec
 * @param result Benchmark result structure
 */
extern void calculateStats(BenchResult *result);

/**
 * Print benchmark results to console
 * @param result Benchmark result structure
 */
extern void printBenchResult(BenchResult *result);

/**
 * Save benchmark group results to JSON
 * Appends results to the global results structure
 * @param groupName Test group name (e.g., "static_files", "uploads")
 * @param results Array of benchmark results
 * @param count Number of results in array
 */
extern void saveBenchGroup(cchar *groupName, BenchResult **results, int count);

/**
 * Save final results to JSON file
 * Writes complete results with metadata to AI/benchmarks/latest.json
 */
extern void saveFinalResults(void);

#ifdef __cplusplus
}
#endif
#endif /* _h_BENCH_UTILS */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
