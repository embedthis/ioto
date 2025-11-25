/*
    bench-utils.c - Benchmark utility functions

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "bench-utils.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>

/*********************************** Locals ***********************************/

// Default durations in milliseconds
#define DEFAULT_TOTAL_DURATION 60000       // 60 seconds total (1 minute)
#define DEFAULT_SOAK_DURATION  6000        // 6 seconds warmup (10%)
#define DEFAULT_BENCH_DURATION 54000       // 54 seconds benchmarking (90%)

static Ticks totalDuration = DEFAULT_TOTAL_DURATION;
static Ticks soakDuration = DEFAULT_SOAK_DURATION;
static Ticks benchDuration = DEFAULT_BENCH_DURATION;
static Ticks perGroupDuration = 0;  // Calculated based on number of groups
static Json  *globalResults = NULL; // Global results JSON structure

/************************************ Code ************************************/

void configureDuration(int numGroups)
{
    char *env = getenv("TESTME_DURATION");
    int  tmDuration;

    if (env) {
        // User specified duration in seconds via tm --duration
        tmDuration = atoi(env);
        if (tmDuration > 0) {
            totalDuration = tmDuration * TPS;  // Convert to milliseconds
        }
    }

    // Allocate 10% to soak, 90% to benchmarking
    soakDuration = totalDuration / 10;
    benchDuration = totalDuration - soakDuration;

    // Divide benchmark time among test groups
    if (numGroups > 0) {
        perGroupDuration = benchDuration / numGroups;
    }

    printf("Duration-based benchmarking:\n");
    printf("  Total: %lld seconds\n", (long long) (totalDuration / 1000));
    printf("  Soak:  %lld seconds\n", (long long) (soakDuration / 1000));
    printf("  Bench: %lld seconds\n", (long long) (benchDuration / 1000));
    printf("  Per group: %lld seconds for %d groups\n", (long long) (perGroupDuration / 1000), numGroups);
}

Ticks getSoakDuration(void)
{
    return soakDuration;
}

Ticks getBenchDuration(void)
{
    return perGroupDuration;
}

BenchResult *createBenchResult(cchar *name)
{
    BenchResult *result;

    result = rAllocType(BenchResult);
    result->name = sclone(name);
    result->iterations = 0;
    result->totalTime = 0;
    result->minTime = MAXINT64;
    result->maxTime = 0;
    result->avgTime = 0.0;
    result->p95Time = 0.0;
    result->p99Time = 0.0;
    result->requestsPerSec = 0.0;
    result->bytesTransferred = 0;
    result->errors = 0;
    result->samples = rAllocList(0, 0);
    return result;
}

void freeBenchResult(BenchResult *result)
{
    if (result) {
        rFree(result->name);
        rFree(result->samples);
        rFree(result);
    }
}

void recordTiming(BenchResult *result, Ticks elapsed)
{
    if (!result) return;

    // Add sample to list for percentile calculations
    rAddItem(result->samples, (void*) (ssize) elapsed);

    // Track running totals
    result->totalTime += elapsed;
    if (elapsed < result->minTime) {
        result->minTime = elapsed;
    }
    if (elapsed > result->maxTime) {
        result->maxTime = elapsed;
    }
}

// Comparison function for qsort
static int compareTicks(const void *a, const void *b)
{
    Ticks ta = (Ticks) (ssize) * (void**) a;
    Ticks tb = (Ticks) (ssize) * (void**) b;

    return (ta > tb) - (ta < tb);
}

void calculateStats(BenchResult *result)
{
    int count, p95Index, p99Index;

    if (!result || !result->samples) return;

    count = rGetListLength(result->samples);
    if (count == 0) {
        return;
    }

    // Calculate average
    result->avgTime = (double) result->totalTime / count;

    // Calculate requests per second
    if (result->totalTime > 0) {
        result->requestsPerSec = (count * 1000.0) / result->totalTime;
    }

    // Sort samples for percentile calculations
    qsort(result->samples->items, (size_t) count, sizeof(void*), compareTicks);

    // Calculate p95 (95th percentile)
    p95Index = (int) (count * 0.95);
    if (p95Index >= count) p95Index = count - 1;
    result->p95Time = (Ticks) (ssize) result->samples->items[p95Index];

    // Calculate p99 (99th percentile)
    p99Index = (int) (count * 0.99);
    if (p99Index >= count) p99Index = count - 1;
    result->p99Time = (Ticks) (ssize) result->samples->items[p99Index];
}

void printBenchResult(BenchResult *result)
{
    if (!result) return;

    printf("\n");
    printf("=== %s ===\n", result->name);
    printf("Iterations:       %d\n", result->iterations);
    printf("Total Time:       %lld ms\n", (long long) result->totalTime);
    printf("Requests/sec:     %.2f\n", result->requestsPerSec);
    printf("Latency (ms):\n");
    printf("  Min:            %lld\n", (long long) result->minTime);
    printf("  Avg:            %.3f\n", result->avgTime);
    printf("  Max:            %lld\n", (long long) result->maxTime);
    printf("  p95:            %.3f\n", result->p95Time);
    printf("  p99:            %.3f\n", result->p99Time);
    if (result->bytesTransferred > 0) {
        printf("Bytes:            %lld (%.2f MB)\n",
               (long long) result->bytesTransferred,
               result->bytesTransferred / (1024.0 * 1024.0));
        printf("Throughput:       %.2f MB/s\n",
               (result->bytesTransferred / (1024.0 * 1024.0)) / (result->totalTime / 1000.0));
    }
    printf("Errors:           %d\n", result->errors);
    printf("\n");
}

void saveBenchGroup(cchar *groupName, BenchResult **results, int count)
{
    Json        *group, *testResult;
    BenchResult *result;
    int         i;

    if (!groupName || !results || count <= 0) return;

    // Initialize global results on first call
    if (!globalResults) {
        globalResults = jsonAlloc();
    }

    // Create group object
    group = jsonAlloc();

    // Add each test result
    for (i = 0; i < count; i++) {
        result = results[i];
        if (!result) continue;

        testResult = jsonAlloc();

        jsonSetNumber(testResult, 0, "iterations", result->iterations);
        jsonSetDouble(testResult, 0, "requestsPerSec", result->requestsPerSec);
        jsonSetDouble(testResult, 0, "avgLatency", result->avgTime);
        jsonSetDouble(testResult, 0, "p95Latency", result->p95Time);
        jsonSetDouble(testResult, 0, "p99Latency", result->p99Time);
        jsonSetNumber(testResult, 0, "minLatency", (int64) result->minTime);
        jsonSetNumber(testResult, 0, "maxLatency", (int64) result->maxTime);
        jsonSetNumber(testResult, 0, "bytesTransferred", (int64) result->bytesTransferred);
        jsonSetNumber(testResult, 0, "errors", result->errors);

        // Blend testResult into group at result->name
        jsonBlend(group, 0, result->name, testResult, 0, NULL, 0);
    }

    // Blend group into global results at groupName
    jsonBlend(globalResults, 0, groupName, group, 0, NULL, 0);
}

/*
   Save results as markdown table
 */
static void saveMarkdownResults(cchar *version, cchar *timestamp, cchar *platform, cchar *profile, cchar *tls)
{
    FILE     *fp;
    JsonNode *groupNode, *testNode;
    int      groupId;
    cchar    *categoryLabel;

    fp = fopen("../../AI/benchmarks/latest.md", "w");
    if (!fp) {
        printf("Warning: Could not open AI/benchmarks/latest.md for writing\n");
        return;
    }

    // Write header
    fprintf(fp, "# Web Server Benchmark Results\n\n");
    fprintf(fp, "**Version:** %s  \n", version);
    fprintf(fp, "**Timestamp:** %s  \n", timestamp);
    fprintf(fp, "**Platform:** %s  \n", platform);
    fprintf(fp, "**Profile:** %s  \n", profile);
    fprintf(fp, "**TLS:** %s  \n", tls);
    fprintf(fp, "**Total Duration:** %lld seconds (%llds soak + %llds bench)\n\n",
            (long long) (totalDuration / 1000), (long long) (soakDuration / 1000),
            (long long) (benchDuration / 1000));

    // Write table header
    fprintf(fp, "## Performance Results\n\n");
    fprintf(fp, "| Category | Test | Req/Sec | Avg Latency (ms) | P95 (ms) | P99 (ms) | "
            "Min (ms) | Max (ms) | Bytes | Errors | Iterations |\n");
    fprintf(fp, "|----------|------|---------|------------------|----------|----------|"
            "----------|----------|-------|--------|------------|\n");

    // Iterate through result groups (top-level children of globalResults)
    for (ITERATE_JSON(globalResults, NULL, groupNode, groupNid)) {
        if (!groupNode->name) continue;

        // Format category name
        if (scmp(groupNode->name, "static_files") == 0) {
            categoryLabel = "**Static Files (URL Library)**";
        } else if (scmp(groupNode->name, "https") == 0) {
            categoryLabel = "**HTTPS (URL Library)**";
        } else if (scmp(groupNode->name, "static_files_raw_http") == 0) {
            categoryLabel = "**Static Files (Raw HTTP)**";
        } else if (scmp(groupNode->name, "static_files_raw_https") == 0) {
            categoryLabel = "**Static Files (Raw HTTPS)**";
        } else if (scmp(groupNode->name, "uploads") == 0) {
            categoryLabel = "**Uploads**";
        } else if (scmp(groupNode->name, "auth") == 0) {
            categoryLabel = "**Auth (Digest)**";
        } else if (scmp(groupNode->name, "actions") == 0) {
            categoryLabel = "**Actions**";
        } else {
            categoryLabel = groupNode->name;
        }

        // Write category header row
        fprintf(fp, "| %s | | | | | | | | | | |\n", categoryLabel);

        // Get node ID for this group to iterate its children
        groupId = jsonGetId(globalResults, 0, groupNode->name);

        // Iterate through tests in this group
        for (ITERATE_JSON_ID(globalResults, groupId, testNode, testNid)) {
            int64  iterations, minLat, maxLat, bytesTransferred, errors;
            double reqPerSec, avgLat, p95Lat, p99Lat, bytesMB;
            char   path[256];

            if (!testNode->name) continue;

            // Get test metrics using the full path (groupName.testName)
            snprintf(path, sizeof(path), "%s.%s.iterations", groupNode->name, testNode->name);
            iterations = jsonGetNum(globalResults, 0, path, 0);
            snprintf(path, sizeof(path), "%s.%s.requestsPerSec", groupNode->name, testNode->name);
            reqPerSec = jsonGetDouble(globalResults, 0, path, 0);
            snprintf(path, sizeof(path), "%s.%s.avgLatency", groupNode->name, testNode->name);
            avgLat = jsonGetDouble(globalResults, 0, path, 0);
            snprintf(path, sizeof(path), "%s.%s.p95Latency", groupNode->name, testNode->name);
            p95Lat = jsonGetDouble(globalResults, 0, path, 0);
            snprintf(path, sizeof(path), "%s.%s.p99Latency", groupNode->name, testNode->name);
            p99Lat = jsonGetDouble(globalResults, 0, path, 0);
            snprintf(path, sizeof(path), "%s.%s.minLatency", groupNode->name, testNode->name);
            minLat = jsonGetNum(globalResults, 0, path, 0);
            snprintf(path, sizeof(path), "%s.%s.maxLatency", groupNode->name, testNode->name);
            maxLat = jsonGetNum(globalResults, 0, path, 0);
            snprintf(path, sizeof(path), "%s.%s.bytesTransferred", groupNode->name, testNode->name);
            bytesTransferred = jsonGetNum(globalResults, 0, path, 0);
            snprintf(path, sizeof(path), "%s.%s.errors", groupNode->name, testNode->name);
            errors = jsonGetNum(globalResults, 0, path, 0);

            // Format bytes
            bytesMB = bytesTransferred / (1024.0 * 1024.0);

            // Write test row
            fprintf(fp, "| | %s | %d | %.2f | %.0f | %.0f | %lld | %lld | ", testNode->name,
                    (int) reqPerSec, avgLat, p95Lat, p99Lat, (long long) minLat, (long long) maxLat);

            if (bytesMB >= 1.0) {
                fprintf(fp, "%.1f MB", bytesMB);
            } else if (bytesTransferred >= 1024) {
                fprintf(fp, "%.1f KB", bytesTransferred / 1024.0);
            } else {
                fprintf(fp, "%lld", (long long) bytesTransferred);
            }
            fprintf(fp, " | %lld | %lld |\n", (long long) errors, (long long) iterations);
        }
    }

    fprintf(fp, "\n## Notes\n\n");
    fprintf(fp, "- **Warm tests**: Reuse connection/socket for all requests\n");
    fprintf(fp, "- **Cold tests**: New connection/socket for each request\n");
    fprintf(fp, "- **Raw tests**: Direct socket I/O bypassing URL library (shows true server performance)\n");
    fprintf(fp, "- **URL Library tests**: Standard HTTP client (includes client overhead)\n");
    fprintf(fp, "- All latency values are in milliseconds\n");
    fprintf(fp, "- Bytes column shows total data transferred during test\n");

    fclose(fp);
    printf("Results saved to: AI/benchmarks/latest.md\n");
}

void saveFinalResults(void)
{
    Json      *root, *config;
    char      *platform, *profile, *output;
    char      timestamp[64];
    time_t    now;
    struct tm *tm_info;

    if (!globalResults) {
        printf("Warning: No benchmark results to save\n");
        return;
    }

    // Create root object with metadata
    root = jsonAlloc();

    // Add version (would need to come from build system)
    jsonSetString(root, 0, "version", "1.0.0-dev");

    // Add timestamp
    time(&now);
    tm_info = gmtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", tm_info);
    jsonSetString(root, 0, "timestamp", timestamp);

    // Add platform info
    platform = getenv("PLATFORM");
    if (!platform) {
#if ME_UNIX_LIKE
        platform = "unix";
#elif MACOSX
        platform = "macosx";
#elif LINUX
        platform = "linux";
#elif WINDOWS
        platform = "windows";
#else
        platform = "unknown";
#endif
    }
    jsonSetString(root, 0, "platform", platform);

    // Add profile
    profile = getenv("PROFILE");
    if (!profile) {
#if ME_DEBUG
        profile = "debug";
#else
        profile = "release";
#endif
    }
    jsonSetString(root, 0, "profile", profile);

    // Add TLS info
    jsonSetString(root, 0, "tls", "openssl");  // Would need runtime detection

    // Add config object
    config = jsonAlloc();
    jsonSetNumber(config, 0, "soakDuration", (int64) soakDuration);
    jsonSetNumber(config, 0, "benchDuration", (int64) benchDuration);
    jsonSetNumber(config, 0, "perGroupDuration", (int64) perGroupDuration);
    jsonSetNumber(config, 0, "totalDuration", (int64) totalDuration);
    jsonSetString(config, 0, "timingPrecision", "milliseconds");
    // Blend config into root
    jsonBlend(root, 0, "config", config, 0, NULL, 0);

    // Blend results into root
    jsonBlend(root, 0, "results", globalResults, 0, NULL, 0);

    // Save to JSON5 file
    output = jsonToString(root, 0, NULL, JSON_PRETTY);
    if (output) {
        FILE *fp = fopen("../../AI/benchmarks/latest.json5", "w");
        if (fp) {
            fprintf(fp, "%s\n", output);
            fclose(fp);
            printf("\nResults saved to: AI/benchmarks/latest.json5\n");
        } else {
            printf("Warning: Could not open AI/benchmarks/latest.json5 for writing\n");
            printf("Results:\n%s\n", output);
        }
        rFree(output);
    }

    // Save to markdown file
    saveMarkdownResults("1.0.0-dev", timestamp, platform, profile, "openssl");

    jsonFree(root);
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
