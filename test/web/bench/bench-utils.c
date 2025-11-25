/*
    bench-utils.c - Benchmark utility functions

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "bench-utils.h"
#include "json.h"
#include "url.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#if MACOSX
#include <mach/mach.h>
#endif

/*********************************** Locals ***********************************/

// Default durations in milliseconds
#define DEFAULT_TOTAL_DURATION 120000      // 120 seconds total (2 minutes)
#define DEFAULT_SOAK_DURATION  12000       // 12 seconds warmup (10%)
#define DEFAULT_BENCH_DURATION 108000      // 108 seconds benchmarking (90%)

static Ticks totalDuration = DEFAULT_TOTAL_DURATION;
static Ticks soakDuration = DEFAULT_SOAK_DURATION;
static Ticks benchDuration = DEFAULT_BENCH_DURATION;
static Ticks perGroupDuration = 0;  // Calculated based on number of groups
static Json  *globalResults = NULL; // Global results JSON structure
static int64 initialMemorySize = 0; // Memory size after soak phase
static int64 finalMemorySize = 0;   // Memory size at benchmark completion
static int   webServerPid = 0;      // PID of web server being benchmarked

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

/*
    Connection Management Functions
 */

ConnectionCtx *createConnectionCtx(bool warm, Ticks timeout)
{
    ConnectionCtx *ctx;

    ctx = rAllocType(ConnectionCtx);
    ctx->up = NULL;
    ctx->sp = NULL;
    ctx->reuseConnection = warm;
    ctx->useSocket = false;
    ctx->useTls = false;
    ctx->timeout = timeout;
    ctx->host = NULL;
    ctx->port = 0;
    return ctx;
}

ConnectionCtx *createSocketCtx(bool warm, Ticks timeout, cchar *host, int port, bool useTls)
{
    ConnectionCtx *ctx;

    ctx = rAllocType(ConnectionCtx);
    ctx->up = NULL;
    ctx->sp = NULL;
    ctx->reuseConnection = warm;
    ctx->useSocket = true;
    ctx->useTls = useTls;
    ctx->timeout = timeout;
    ctx->host = sclone(host);
    ctx->port = port;
    return ctx;
}

Url *getConnection(ConnectionCtx *ctx)
{
    if (!ctx || ctx->useSocket) {
        return NULL;
    }
    // For warm connections, reuse existing connection
    if (ctx->reuseConnection && ctx->up) {
        return ctx->up;
    }
    // For cold connections or first warm connection, allocate new
    if (!ctx->up) {
        ctx->up = urlAlloc(URL_NO_LINGER);
        urlSetTimeout(ctx->up, ctx->timeout);
    }
    return ctx->up;
}

RSocket *getSocket(ConnectionCtx *ctx)
{
    Ticks deadline;

    if (!ctx || !ctx->useSocket) {
        return NULL;
    }
    // For warm connections, reuse existing socket
    if (ctx->reuseConnection && ctx->sp) {
        return ctx->sp;
    }
    // For cold connections or first warm connection, allocate new
    if (!ctx->sp) {
        ctx->sp = rAllocSocket();
        if (ctx->useTls) {
            rSetTls(ctx->sp);
        }
        rSetSocketLinger(ctx->sp, 0);
        deadline = rGetTicks() + ctx->timeout;
        if (rConnectSocket(ctx->sp, ctx->host, ctx->port, deadline) < 0) {
            rFreeSocket(ctx->sp);
            ctx->sp = NULL;
            return NULL;
        }
    }
    return ctx->sp;
}

void releaseConnection(ConnectionCtx *ctx)
{
    if (!ctx) {
        return;
    }
    if (ctx->sp && ctx->sp->fd == INVALID_SOCKET) {
        rFreeSocket(ctx->sp);
        ctx->sp = NULL;
        return;
    }
    // For cold connections, close and free
    if (!ctx->reuseConnection) {
        if (ctx->useSocket) {
            if (ctx->sp) {
                rFreeSocket(ctx->sp);
                ctx->sp = NULL;
            }
        } else {
            if (ctx->up) {
                urlClose(ctx->up);
                urlFree(ctx->up);
                ctx->up = NULL;
            }
        }
    }
    // For warm connections, keep connection open
}

void freeConnectionCtx(ConnectionCtx *ctx)
{
    if (!ctx) {
        return;
    }
    if (ctx->sp) {
        rFreeSocket(ctx->sp);
    }
    if (ctx->up) {
        urlClose(ctx->up);
        urlFree(ctx->up);
    }
    rFree((char*) ctx->host);
    rFree(ctx);
}

RequestResult executeRequest(ConnectionCtx *ctx, cchar *method, cchar *url,
                              cchar *data, size_t dataLen, cchar *headers)
{
    RequestResult result;
    Url           *up;
    Ticks         startTime;
    cchar         *response;

    result.status = 0;
    result.bytes = 0;
    result.elapsed = 0;
    result.success = false;

    up = getConnection(ctx);
    if (!up) {
        return result;
    }
    startTime = rGetTicks();
    result.status = urlFetch(up, method, url, data, dataLen, headers);

    /*
        Consume response to enable connection reuse and complete full request timing
        Use urlGetResponse() return value for bytes since rxLen is -1 for chunked encoding
    */
    response = urlGetResponse(up);
    result.elapsed = rGetTicks() - startTime;
    result.bytes = response ? slen(response) : 0;

    // Check success based on method
    if (smatch(method, "GET") || smatch(method, "HEAD")) {
        result.success = (result.status == 200);
    } else if (smatch(method, "POST")) {
        result.success = (result.status == 200 || result.status == 201);
    } else if (smatch(method, "PUT")) {
        result.success = (result.status == 200 || result.status == 201 || result.status == 204);
    } else if (smatch(method, "DELETE")) {
        result.success = (result.status == 200 || result.status == 204);
    } else {
        result.success = (result.status >= 200 && result.status < 300);
    }
    releaseConnection(ctx);
    return result;
}

RequestResult executeRawRequest(ConnectionCtx *ctx, cchar *request, ssize expectedSize)
{
    RequestResult result;
    RSocket       *sp;
    char          headers[8192], *body;
    ssize         bodyStart, bodyInHeaders, headerLen, contentLen, bodyRead, nbytes;
    Ticks         deadline;
    bool          success;

    deadline = rGetTicks() + ctx->timeout;
    success = false;

    result.status = 0;
    result.bytes = expectedSize;
    result.elapsed = 0;
    result.success = false;

    sp = getSocket(ctx);
    if (!sp) {
        tinfo("Raw socket connect failed: %s:%d", ctx->host, ctx->port);
        return result;
    }
    // Send request
    if (rWriteSocket(sp, request, slen(request), deadline) < 0) {
        tinfo("Raw socket write failed: %s", rGetSocketError(sp));
        if (ctx->reuseConnection) {
            rCloseSocket(sp);
        }
        return result;
    }
    // Read headers (may also read some body data)
    headerLen = readHeaders(sp, headers, sizeof(headers), deadline, &bodyStart, &bodyInHeaders);
    if (headerLen < 0) {
        tinfo("Raw socket read headers failed: %s", rGetSocketError(sp));
        if (ctx->reuseConnection) {
            rCloseSocket(sp);
        }
        return result;
    }
    // Parse Content-Length
    contentLen = parseContentLength(headers, (size_t) bodyStart);
    if (contentLen < 0 || contentLen > expectedSize * 2) {
        if (ctx->reuseConnection) {
            rCloseSocket(sp);
        }
        return result;
    }
    // Read body
    body = rAlloc((size_t) (contentLen + 1));
    bodyRead = 0;

    // Copy any body data that was read with headers
    if (bodyInHeaders > 0) {
        memcpy(body, headers + bodyStart, (size_t) bodyInHeaders);
        bodyRead = bodyInHeaders;
    }

    // Read remaining body data
    while (bodyRead < contentLen) {
        nbytes = rReadSocket(sp, body + bodyRead, (size_t) (contentLen - bodyRead), deadline);
        if (nbytes <= 0) {
            tinfo("Raw socket read body failed: %s (read %lld of %lld bytes)",
                  rGetSocketError(sp), (int64) bodyRead, (int64) contentLen);
            rFree(body);
            if (ctx->reuseConnection) {
                rCloseSocket(sp);
            }
            return result;
        }
        bodyRead += nbytes;
    }
    rFree(body);

    // Handle connection cleanup based on server response
    if (scontains(headers, "Connection: close")) {
        // Server requested close - reset socket for reuse
        rCloseSocket(sp);
        rResetSocket(sp);
    }
    releaseConnection(ctx);
    success = true;

    result.status = success ? 200 : 0;
    result.success = success;
    return result;
}

/*
    Error Reporting Functions
 */

void logRequestError(cchar *benchName, cchar *url, int status, int errorCount, bool recordResults)
{
    if (errorCount <= 5 || !recordResults) {
        tinfo("Warning: %s request failed: %s (status %d)", benchName, url, status);
    }
}

/*
    BenchContext Functions - Unified Result Processing

    These functions consolidate the repeated error handling and result recording
    patterns across benchmark functions.
 */

void initBenchContext(BenchContext *ctx, cchar *category, cchar *description)
{
    // Preserve global state
    bool fatalError = ctx->fatalError;
    bool stopOnErrors = ctx->stopOnErrors;
    bool recordResults = ctx->recordResults;
    int totalErrors = ctx->totalErrors;

    // Reset per-benchmark state
    memset(ctx, 0, sizeof(BenchContext));

    // Restore global state
    ctx->fatalError = fatalError;
    ctx->stopOnErrors = stopOnErrors;
    ctx->recordResults = recordResults;
    ctx->totalErrors = totalErrors;

    // Set per-benchmark configuration
    ctx->category = category;
    if (recordResults && description) {
        tinfo("%s", description);
    }
}

bool processResponse(BenchContext *ctx, RequestResult *result, cchar *url, Ticks startTime)
{
    // Calculate elapsed time and determine success
    result->elapsed = rGetTicks() - startTime;
    result->success = (result->status == 0 || (result->status >= 200 && result->status < 300));

    ctx->totalRequests++;

    if (!result->success) {
        ctx->errorCount++;
        ctx->totalErrors++;
        logRequestError(ctx->category, url, result->status, ctx->errorCount, ctx->recordResults);

        if (ctx->stopOnErrors) {
            ctx->fatalError = true;
            if (ctx->connCtx) {
                freeConnectionCtx(ctx->connCtx);
                ctx->connCtx = NULL;
            }
            return false;
        }
    }
    if (ctx->recordResults) {
        recordRequest(ctx->results[ctx->resultOffset + ctx->classIndex], result->success, result->elapsed, ctx->bytes);
    }
    return true;
}

void finishBenchContext(BenchContext *ctx, int count, cchar *groupName)
{
    if (ctx->errorCount > 0) {
        tinfo("Warning: %s benchmark had %d errors out of %d requests (%.1f%%)",
              ctx->category, ctx->errorCount, ctx->totalRequests,
              (ctx->errorCount * 100.0) / ctx->totalRequests);
    }
    if (ctx->recordResults && count > 0 && groupName) {
        finalizeResults(ctx->results, count, groupName);
    }
}

/*
   Get process memory size in bytes for a specific PID
   Returns the resident set size (RSS) of the specified process
   If pid is 0, returns memory of current process
 */
int64 getProcessMemorySize(int pid)
{
#if MACOSX
    if (pid == 0) {
        // Current process
        struct mach_task_basic_info info;
        mach_msg_type_number_t      infoCount = MACH_TASK_BASIC_INFO_COUNT;

        if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t) &info, &infoCount) == KERN_SUCCESS) {
            return (int64) info.resident_size;
        }
    } else {
        // Other process - use ps command as task_for_pid requires special entitlements
        char   cmd[256];
        FILE   *fp;
        int64  rss = 0;

        snprintf(cmd, sizeof(cmd), "ps -o rss= -p %d 2>/dev/null", pid);
        fp = popen(cmd, "r");
        if (fp) {
            if (fscanf(fp, "%lld", &rss) == 1) {
                pclose(fp);
                return rss * 1024;  // ps reports in KB, convert to bytes
            }
            pclose(fp);
        }
    }
    return 0;
#elif LINUX
    if (pid == 0) {
        // Current process
        struct rusage usage;

        if (getrusage(RUSAGE_SELF, &usage) == 0) {
            return (int64) usage.ru_maxrss * 1024;
        }
    } else {
        // Other process - read from /proc
        char   path[256];
        FILE   *fp;
        char   line[512];
        int64  rss = 0;

        snprintf(path, sizeof(path), "/proc/%d/status", pid);
        fp = fopen(path, "r");
        if (fp) {
            while (fgets(line, sizeof(line), fp)) {
                if (strncmp(line, "VmRSS:", 6) == 0) {
                    sscanf(line + 6, "%lld", &rss);
                    fclose(fp);
                    return rss * 1024;  // /proc reports in KB
                }
            }
            fclose(fp);
        }
    }
    return 0;
#else
    struct rusage usage;

    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return (int64) usage.ru_maxrss;
    }
    return 0;
#endif
}

/*
   Find the web server process PID by reading from bench.pid file created by setup.sh
   Returns PID or 0 if not found
 */
static int findWebServerPid(void)
{
    FILE *fp;
    int  pid = 0;

    // Read PID from file created by setup.sh
    fp = fopen("bench.pid", "r");
    if (fp) {
        if (fscanf(fp, "%d", &pid) == 1) {
            fclose(fp);
            return pid;
        }
        fclose(fp);
    }
    return 0;
}

void recordInitialMemory(void)
{
    if (webServerPid == 0) {
        webServerPid = findWebServerPid();
        if (webServerPid == 0) {
            printf("Warning: Could not find web server process (port 4260)\n");
            return;
        }
        printf("Monitoring web server process: PID %d\n", webServerPid);
    }
    initialMemorySize = getProcessMemorySize(webServerPid);
    if (initialMemorySize > 0) {
        printf("Initial web server memory: %.2f MB\n", initialMemorySize / (1024.0 * 1024.0));
    } else {
        printf("Warning: Could not read memory for web server PID %d\n", webServerPid);
    }
}

void recordFinalMemory(void)
{
    if (webServerPid == 0) {
        printf("Warning: Web server PID not set\n");
        return;
    }
    finalMemorySize = getProcessMemorySize(webServerPid);
    if (finalMemorySize > 0) {
        printf("Final web server memory: %.2f MB\n", finalMemorySize / (1024.0 * 1024.0));
    } else {
        printf("Warning: Could not read memory for web server PID %d\n", webServerPid);
    }
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
        // Reset min to 0 when no samples (avoid displaying MAXINT64)
        result->minTime = 0;
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
    if (result->iterations == 0) return;  // Skip empty results after failure

    printf("\n");
    printf("=== %s ===\n", result->name);
    printf("Iterations:       %d\n", result->iterations);
    printf("Total Time:       %lld ms\n", (long long) result->totalTime);
    printf("Requests/sec:     %.2f\n", result->requestsPerSec);
    printf("Latency (ms):\n");
    printf("  Min:            %.2f\n", (double) result->minTime);
    printf("  Avg:            %.3f\n", result->avgTime);
    printf("  Max:            %.2f\n", (double) result->maxTime);
    printf("  p95:            %.2f\n", result->p95Time);
    printf("  p99:            %.2f\n", result->p99Time);
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

    fp = fopen("../../doc/benchmarks/latest.md", "w");
    if (!fp) {
        printf("Warning: Could not open doc/benchmarks/latest.md for writing\n");
        return;
    }

    // Write header
    fprintf(fp, "# Web Server Benchmark Results\n\n");
    fprintf(fp, "**Version:** %s  \n", version);
    fprintf(fp, "**Timestamp:** %s  \n", timestamp);
    fprintf(fp, "**Platform:** %s  \n", platform);
    fprintf(fp, "**Profile:** %s  \n", profile);
    fprintf(fp, "**TLS:** %s  \n", tls);
    fprintf(fp, "**Total Duration:** %lld seconds (%llds soak + %llds bench)  \n",
            (long long) (totalDuration / 1000), (long long) (soakDuration / 1000),
            (long long) (benchDuration / 1000));
    fprintf(fp, "**Initial Memory (after soak):** %.2f MB  \n", initialMemorySize / (1024.0 * 1024.0));
    fprintf(fp, "**Final Memory:** %.2f MB  \n", finalMemorySize / (1024.0 * 1024.0));
    fprintf(fp, "**Memory Delta:** %+.2f MB\n\n", (finalMemorySize - initialMemorySize) / (1024.0 * 1024.0));

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
        } else if (scmp(groupNode->name, "websockets") == 0) {
            categoryLabel = "**WebSockets**";
        } else if (scmp(groupNode->name, "put") == 0) {
            categoryLabel = "**PUT Uploads**";
        } else if (scmp(groupNode->name, "multipart_upload") == 0) {
            categoryLabel = "**Multipart Uploads**";
        } else if (scmp(groupNode->name, "auth") == 0) {
            categoryLabel = "**Auth (Digest)**";
        } else if (scmp(groupNode->name, "actions") == 0) {
            categoryLabel = "**Actions**";
        } else if (scmp(groupNode->name, "mixed") == 0) {
            categoryLabel = "**Mixed Workload**";
        } else if (scmp(groupNode->name, "max_throughput") == 0) {
            categoryLabel = "**Max Throughput**";
        } else if (scmp(groupNode->name, "uploads") == 0) {
            categoryLabel = "**Uploads**";
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
            fprintf(fp, "| | %s | %d | %.2f | %.1f | %.1f | %.1f | %.1f | ", testNode->name,
                    (int) reqPerSec, avgLat, p95Lat, p99Lat, (double) minLat, (double) maxLat);

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
    fprintf(fp, "- **Max Throughput test**: Uses wrk benchmark tool with 12 threads, 40 connections, 30 second duration\n");
    fprintf(fp, "- **All other tests**: Run with 1 CPU core for the server and 1 CPU core for the client\n");
    fprintf(fp, "- **Warm tests**: Reuse connection/socket for all requests\n");
    fprintf(fp, "- **Cold tests**: New connection/socket for each request\n");
    fprintf(fp, "- **Raw tests**: Direct socket I/O bypassing URL library (shows true server performance)\n");
    fprintf(fp, "- **URL Library tests**: Standard HTTP client (includes client overhead)\n");
    fprintf(fp, "- All latency values are in milliseconds\n");
    fprintf(fp, "- Bytes column shows total data transferred during test\n");

    fclose(fp);
    printf("Results saved to: doc/benchmarks/latest.md\n");
}

void saveFinalResults(void)
{
    Json      *root, *config;
    char      *platform, *profile, *output, *osver, *machine, platformInfo[256];
    char      timestamp[64];
    FILE      *fp;
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
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S UTC", tm_info);
    jsonSetString(root, 0, "timestamp", timestamp);

    // Add platform info with OS version and machine type
    platform = getenv("PLATFORM");
    if (!platform) {
#if MACOSX
        platform = "macosx";
#elif LINUX
        platform = "linux";
#elif WINDOWS
        platform = "windows";
#elif ME_UNIX_LIKE
        platform = "unix";
#else
        platform = "unknown";
#endif
    }

    // Get OS version and machine type
    osver = NULL;
    machine = NULL;

#if MACOSX || LINUX || ME_UNIX_LIKE
    // Get OS version via uname -r
    fp = popen("uname -r 2>/dev/null", "r");
    if (fp) {
        osver = rAlloc(128);
        if (fgets(osver, 128, fp)) {
            // Trim newline
            osver[strcspn(osver, "\n")] = '\0';
        }
        pclose(fp);
    }

    // Get machine type via uname -m
    fp = popen("uname -m 2>/dev/null", "r");
    if (fp) {
        machine = rAlloc(128);
        if (fgets(machine, 128, fp)) {
            // Trim newline
            machine[strcspn(machine, "\n")] = '\0';
        }
        pclose(fp);
    }
#endif

    // Build platform info string
    if (osver && machine) {
        snprintf(platformInfo, sizeof(platformInfo), "%s %s (%s)", platform, osver, machine);
    } else if (osver) {
        snprintf(platformInfo, sizeof(platformInfo), "%s %s", platform, osver);
    } else {
        snprintf(platformInfo, sizeof(platformInfo), "%s", platform);
    }
    jsonSetString(root, 0, "platform", platformInfo);

    // Cleanup
    if (osver) {
        rFree(osver);
    }
    if (machine) {
        rFree(machine);
    }

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
    jsonSetNumber(config, 0, "initialMemoryBytes", initialMemorySize);
    jsonSetNumber(config, 0, "finalMemoryBytes", finalMemorySize);
    // Blend config into root
    jsonBlend(root, 0, "config", config, 0, NULL, 0);

    // Blend results into root
    jsonBlend(root, 0, "results", globalResults, 0, NULL, 0);

    // Save to JSON5 file
    output = jsonToString(root, 0, NULL, JSON_PRETTY);
    if (output) {
        FILE *fp = fopen("../../doc/benchmarks/latest.json5", "w");
        if (fp) {
            fprintf(fp, "%s\n", output);
            fclose(fp);
            printf("\nResults saved to: doc/benchmarks/latest.json5\n");
        } else {
            printf("Warning: Could not open doc/benchmarks/latest.json5 for writing\n");
            printf("Results:\n%s\n", output);
        }
        rFree(output);
    }

    // Save to markdown file
    saveMarkdownResults("1.0.0-dev", timestamp, platformInfo, profile, "openssl");

    jsonFree(root);
}

/*
    Result Management Functions
 */

BenchResult *initResult(cchar *name, bool recordResults, cchar *description)
{
    if (recordResults && description) {
        tinfo("%s", description);
    }
    return recordResults ? createBenchResult(name) : NULL;
}

void recordRequest(BenchResult *result, bool isSuccess, Ticks elapsed, ssize bytes)
{
    if (!result) {
        return;
    }
    result->iterations++;
    if (isSuccess) {
        recordTiming(result, elapsed);
        result->bytesTransferred += bytes;
    } else {
        result->errors++;
        if (bctx) {
            bctx->totalErrors++;
            if (bctx->stopOnErrors) {
                bctx->fatalError = true;
                ttrue(false, "TESTME_STOP: Stopping benchmark due to request error");
            }
        }
    }
}

void finalizeResults(BenchResult **results, int count, cchar *groupName)
{
    int i;

    // Calculate and print statistics
    for (i = 0; i < count; i++) {
        if (results[i]) {
            calculateStats(results[i]);
            printBenchResult(results[i]);
        }
    }

    // Save results
    saveBenchGroup(groupName, results, count);

    // Cleanup
    for (i = 0; i < count; i++) {
        freeBenchResult(results[i]);
    }
}

/*
    Raw Socket Utilities
 */

/*
   Extract Content-Length from raw HTTP headers
   Returns -1 if not found or invalid
 */
ssize parseContentLength(cchar *headers, size_t len)
{
    cchar  *start, *end, *line, *value;
    size_t remaining;

    start = headers;
    remaining = len;

    while (remaining > 0) {
        line = start;
        end = (cchar*) memchr(line, '\n', remaining);
        if (!end) {
            break;
        }
        // Check for end of headers
        if (end == line || (end > line && *(end - 1) == '\r' && end - 1 == line)) {
            break;
        }
        // Check for Content-Length header (case insensitive)
        if ((end - line) > 16 && sncaselesscmp(line, "content-length:", 15) == 0) {
            value = line + 15;
            // Skip whitespace
            while (value < end && (*value == ' ' || *value == '\t')) {
                value++;
            }
            return stoi(value);
        }
        remaining -= (size_t) (end - line + 1);
        start = end + 1;
    }
    return -1;
}

/*
   Read until we find \r\n\r\n (end of headers)
   Returns total bytes read, or -1 on error
   Sets *bodyStart to the offset where body data begins (after the header delimiter)
   Sets *bodyLen to how much body data was read along with headers
 */
ssize readHeaders(RSocket *sp, char *buf, size_t bufsize, Ticks deadline, ssize *bodyStart, ssize *bodyLen)
{
    ssize total, nbytes, headerEnd;
    char  *end;

    total = 0;
    *bodyStart = 0;
    *bodyLen = 0;

    while (total < (ssize) bufsize - 1) {
        nbytes = rReadSocket(sp, buf + total, bufsize - (size_t) total - 1, deadline);
        if (nbytes <= 0) {
            return -1;
        }
        total += nbytes;
        buf[total] = '\0';

        // Look for end of headers
        end = scontains(buf, "\r\n\r\n");
        if (end) {
            headerEnd = end - buf + 4;  // +4 for \r\n\r\n
            *bodyStart = headerEnd;
            *bodyLen = total - headerEnd;
            return total;
        }
        // Also accept \n\n (non-standard but some clients use it)
        end = scontains(buf, "\n\n");
        if (end) {
            headerEnd = end - buf + 2;  // +2 for \n\n
            *bodyStart = headerEnd;
            *bodyLen = total - headerEnd;
            return total;
        }
    }
    return -1;  // Headers too large
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
