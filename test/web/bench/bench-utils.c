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
#if !WINDOWS
#include <sys/resource.h>
#endif
#if MACOSX
#include <mach/mach.h>
#endif
#if WINDOWS
#include <psapi.h>
#endif

/*********************************** Locals ***********************************/

// Default durations in milliseconds
#define DEFAULT_TOTAL_DURATION 120000      // 120 seconds total (2 minutes)
#define DEFAULT_SOAK_DURATION  12000       // 12 seconds warmup (10%)
#define DEFAULT_BENCH_DURATION 108000      // 108 seconds benchmarking (90%)

static Ticks totalDuration = DEFAULT_TOTAL_DURATION;
static Ticks soakDuration = DEFAULT_SOAK_DURATION;
static Ticks benchDuration = DEFAULT_BENCH_DURATION;
static Ticks perGroupDuration = 0;                    // Calculated based on number of groups
static Json  *globalResults = NULL;                   // Global results JSON structure
static int64 initialMemorySize = 0;                   // Memory size after soak phase
static int64 finalMemorySize = 0;                     // Memory size at benchmark completion
static int   webServerPid = 0;                        // PID of web server being benchmarked
static cchar *reportName = NULL;                      // Report filename (without extension)

/*
    Iteration count configuration per file size class
    Multiplier relative to base benchmark iterations
 */
FileClass fileClasses[] = {
    { "1KB",   "static/1K.txt",   1024,       1.0  }, // Full iterations for small files
    { "10KB",  "static/10K.txt",  10240,      1.0  }, // Full iterations
    { "100KB", "static/100K.txt", 102400,     0.25 }, // 25% iterations for large files
    { "1MB",   "static/1M.txt",   1048576,    0.25 }, // 25% iterations for very large files
    { NULL,    NULL,              0,          0    }
};

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

    // Get report filename from TESTME_REPORT, default to "latest"
    reportName = getenv("TESTME_REPORT");
    if (!reportName || !*reportName) {
        reportName = "latest";
    }

    // Allocate 10% to soak, 90% to benchmarking
    soakDuration = totalDuration / 10;
    benchDuration = totalDuration - soakDuration;

    // Divide benchmark time among test groups
    if (numGroups > 0) {
        perGroupDuration = benchDuration / numGroups;
    }

    printf("Duration-based benchmarking:\n");
    printf("  Report: %s\n", reportName);
    printf("  Total: %lld seconds\n", (long long) (totalDuration / 1000));
    printf("  Soak:  %lld seconds\n", (long long) (soakDuration / 1000));
    printf("  Bench: %lld seconds\n", (long long) (benchDuration / 1000));
    printf("  Per group: %.1f seconds for %d groups\n", perGroupDuration / 1000.0, numGroups);
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
    File Class Utility Functions
 */

void setupTotalUnits(BenchContext *ctx, Ticks duration, bool warmCold)
{
    ctx->duration = duration;
    ctx->totalUnits = 0;
    for (int i = 0; fileClasses[i].name; i++) {
        ctx->totalUnits += fileClasses[i].multiplier;
    }
    if (warmCold) {
        ctx->totalUnits *= 2;  // Account for warm and cold tests
    }
}

Ticks getGroupDuration(FileClass *fc)
{
    Ticks duration;

    duration = (Ticks) (bctx->duration * fc->multiplier / bctx->totalUnits);
    if (duration < MIN_GROUP_DURATION_MS) {
        duration = MIN_GROUP_DURATION_MS;
    }
    return duration;
}

int getColdIterationLimit(FileClass *fc, double totalUnits)
{
    int limit;

    // Divide totalUnits by 2 since setupTotalUnits doubles for warm/cold
    // We only want the cold portion of the budget
    limit = (int) (BENCH_MAX_COLD_ITERATIONS * fc->multiplier / (totalUnits / 2));
    if (limit < 100) {
        limit = 100;  // Minimum iterations per class
    }
    return limit;
}

Ticks calcEqualDuration(Ticks duration, int numGroups)
{
    Ticks d = duration / numGroups;

    return d < MIN_GROUP_DURATION_MS ? MIN_GROUP_DURATION_MS : d;
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
        ctx->up = urlAlloc(0);
        urlSetTimeout(ctx->up, ctx->timeout);
    }
    return ctx->up;
}

RSocket *getSocket(ConnectionCtx *ctx)
{
    Ticks deadline;
    void  *newSession;

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
            // Apply cached session for TLS resumption on cold connections
            if (ctx->session) {
                rSetTlsSession(ctx->sp, ctx->session);
            }
        }
        // rSetSocketLinger(ctx->sp, 0);
        deadline = rGetTicks() + ctx->timeout;
        if (rConnectSocket(ctx->sp, ctx->host, ctx->port, deadline) < 0) {
            rFreeSocket(ctx->sp);
            ctx->sp = NULL;
            return NULL;
        }
        // Cache session after successful TLS connection for future cold connections
        if (ctx->useTls && !ctx->reuseConnection) {
            newSession = rGetTlsSession(ctx->sp);
            if (newSession) {
                if (ctx->session) {
                    rFreeTlsSession(ctx->session);
                }
                ctx->session = newSession;
            }
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
    if (ctx->session) {
        rFreeTlsSession(ctx->session);
    }
    rFree((char*) ctx->host);
    rFree(ctx);
}

/*
    Read the entire response using urlRead into a standard sized buffer.
    Returns the number of bytes read, or -1 on error.
 */
static ssize getResponse(Url *up)
{
    ssize nbytes, total;
    char  buf[ME_BUFSIZE];

    if (!up) {
        return -1;
    }
    if (urlFinalize(up) < 0) {
        return -1;
    }
    total = 0;
    while ((nbytes = urlRead(up, buf, sizeof(buf))) > 0) {
        total += nbytes;
    }
    if (nbytes < 0) {
        return -1;
    }
    return total;
}

RequestResult executeRequest(ConnectionCtx *ctx, cchar *method, cchar *url,
                             cchar *data, size_t dataLen, cchar *headers)
{
    RequestResult result;
    Url           *up;
    Ticks         startTime;
    ssize         bytes;

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
     */
    bytes = getResponse(up);
    result.elapsed = rGetTicks() - startTime;
    result.bytes = bytes > 0 ? bytes : 0;

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
    char          buf[ME_BUFSIZE], headers[8192];
    ssize         bodyStart, bodyInHeaders, headerLen, contentLen, bodyRead, nbytes, toRead;
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
    // Read and discard body using fixed buffer (body data not needed)
    // Body data already read with headers is discarded, just account for bytes
    bodyRead = bodyInHeaders;

    // Read and discard remaining body data
    while (bodyRead < contentLen) {
        toRead = contentLen - bodyRead;
        if (toRead > (ssize) sizeof(buf)) {
            toRead = sizeof(buf);
        }
        nbytes = rReadSocket(sp, buf, (size_t) toRead, deadline);
        if (nbytes <= 0) {
            tinfo("Raw socket read body failed: %s (read %lld of %lld bytes)",
                  rGetSocketError(sp), (int64) bodyRead, (int64) contentLen);
            if (ctx->reuseConnection) {
                rCloseSocket(sp);
            }
            return result;
        }
        bodyRead += nbytes;
    }

    // Handle connection cleanup based on server response
    if (scontains(headers, "Connection: close")) {
        // Server requested close - reset socket for reuse
        rCloseSocket(sp);
        // rResetSocket(sp);
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

void logRequestError(cchar *benchName, cchar *url, int status, int errorCount, bool soak)
{
    if (errorCount <= 5 || soak) {
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
    bool fatal = ctx->fatal;
    bool stopOnErrors = ctx->stopOnErrors;
    bool soak = ctx->soak;
    int  errors = ctx->errors;

    // Reset per-benchmark state
    memset(ctx, 0, sizeof(BenchContext));

    // Restore global state
    ctx->fatal = fatal;
    ctx->stopOnErrors = stopOnErrors;
    ctx->soak = soak;
    ctx->errors = errors;

    // Set per-benchmark configuration
    ctx->category = category;
    if (!soak && description) {
        tinfo("%s", description);
    }
}

bool processResponse(BenchContext *ctx, RequestResult *result, cchar *url, Ticks startTime)
{
    // Calculate elapsed time and determine success
    result->elapsed = rGetTicks() - startTime;
    result->success = (result->status >= 200 && result->status < 300);
    ctx->totalRequests++;

    if (!result->success) {
        ctx->errorCount++;
        ctx->errors++;
        logRequestError(ctx->category, url, result->status, ctx->errorCount, ctx->soak);

        if (ctx->stopOnErrors) {
            ctx->fatal = true;
            if (ctx->connCtx) {
                freeConnectionCtx(ctx->connCtx);
                ctx->connCtx = NULL;
            }
            return false;
        }
    }
    if (!ctx->soak) {
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
    if (!ctx->soak && count > 0 && groupName) {
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
        char  cmd[256];
        FILE  *fp;
        int64 rss = 0;

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
        char  path[256];
        FILE  *fp;
        char  line[512];
        int64 rss = 0;

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
#elif WINDOWS
    PROCESS_MEMORY_COUNTERS pmc;
    HANDLE                  hProcess;
    int64                   memSize;

    memSize = 0;
    if (pid == 0) {
        hProcess = GetCurrentProcess();
        if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
            memSize = (int64) pmc.WorkingSetSize;
        }
    } else {
        hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, (DWORD) pid);
        if (hProcess) {
            if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                memSize = (int64) pmc.WorkingSetSize;
            }
            CloseHandle(hProcess);
        }
    }
    return memSize;
#else
    // Fallback - no memory info available
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
            tinfo("Warning: Could not find web server process (port 4260)\n");
            return;
        }
        tinfo("Monitoring web server process: PID %d\n", webServerPid);
    }
    initialMemorySize = getProcessMemorySize(webServerPid);
    if (initialMemorySize > 0) {
        tinfo("Initial web server memory: %.2f MB\n", initialMemorySize / (1024.0 * 1024.0));
    } else {
        tinfo("Warning: Could not read memory for web server PID %d\n", webServerPid);
    }
}

void recordFinalMemory(void)
{
    if (webServerPid == 0) {
        tinfo("Warning: Web server PID not set\n");
        return;
    }
    finalMemorySize = getProcessMemorySize(webServerPid);
    if (finalMemorySize > 0) {
        tinfo("Final web server memory: %.2f MB\n", finalMemorySize / (1024.0 * 1024.0));
    } else {
        tinfo("Warning: Could not read memory for web server PID %d\n", webServerPid);
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
    fflush(stdout);
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

        jsonSetDouble(testResult, 0, "requestsPerSec", result->requestsPerSec);
        jsonSetDouble(testResult, 0, "avgLatency", result->avgTime);
        jsonSetDouble(testResult, 0, "p95Latency", result->p95Time);
        jsonSetDouble(testResult, 0, "p99Latency", result->p99Time);
        jsonSetNumber(testResult, 0, "minLatency", (int64) result->minTime);
        jsonSetNumber(testResult, 0, "maxLatency", (int64) result->maxTime);
        jsonSetNumber(testResult, 0, "bytesTransferred", (int64) result->bytesTransferred);
        jsonSetNumber(testResult, 0, "iterations", result->iterations);
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
static void saveMarkdownResults(cchar *version, cchar *timestamp, cchar *platform, cchar *profile, cchar *tls,
                                cchar *basePlatform)
{
    FILE     *fp;
    JsonNode *groupNode, *testNode;
    int      groupId;
    cchar    *categoryLabel;
    char     path[256];

    snprintf(path, sizeof(path), "../../doc/benchmarks/%s/%s.md", basePlatform, reportName);
    fp = fopen(path, "w");
    if (!fp) {
        printf("Warning: Could not open doc/benchmarks/%s/%s.md for writing\n", basePlatform, reportName);
        return;
    }

    // Write header
    fprintf(fp, "# Web Server Benchmark Results\n\n");
    fprintf(fp, "## System Configuration\n\n");
    fprintf(fp, "- **Version:** %s\n", version);
    fprintf(fp, "- **Timestamp:** %s\n", timestamp);
    fprintf(fp, "- **Platform:** %s\n", platform);
    fprintf(fp, "- **Profile:** %s\n", profile);
    fprintf(fp, "- **TLS:** %s\n", tls);
    fprintf(fp, "- **Total Duration:** %lld seconds (%llds soak + %llds bench)\n",
            (long long) (totalDuration / 1000), (long long) (soakDuration / 1000),
            (long long) (benchDuration / 1000));
    fprintf(fp, "- **Initial Memory (after soak):** %.2f MB\n", initialMemorySize / (1024.0 * 1024.0));
    fprintf(fp, "- **Final Memory:** %.2f MB\n", finalMemorySize / (1024.0 * 1024.0));
    fprintf(fp, "- **Memory Delta:** %+.2f MB\n", (finalMemorySize - initialMemorySize) / (1024.0 * 1024.0));

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
        } else if (scmp(groupNode->name, "throughput") == 0) {
            categoryLabel = "**Throughput**";
        } else if (scmp(groupNode->name, "single_thread") == 0) {
            categoryLabel = "**Single Thread**";
        } else if (scmp(groupNode->name, "uploads") == 0) {
            categoryLabel = "**Uploads**";
        } else if (scmp(groupNode->name, "upload") == 0) {
            categoryLabel = "**Multipart Uploads**";
        } else if (scmp(groupNode->name, "connections") == 0) {
            categoryLabel = "**Connections**";
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
    fprintf(fp,
            "- **Max Throughput test**: Uses wrk benchmark tool with 12 threads, 40 connections\n");
    fprintf(fp, "- **All other tests**: Run with 1 CPU core for the server and 1 CPU core for the client\n");
    fprintf(fp, "- **Warm tests**: Reuse connection/socket for all requests\n");
    fprintf(fp, "- **Cold tests**: New connection/socket for each request\n");
    fprintf(fp, "- **Raw tests**: Direct socket I/O bypassing URL library (shows true server performance)\n");
    fprintf(fp, "- **URL Library tests**: Standard HTTP client (includes client overhead)\n");
    fprintf(fp, "- All latency values are in milliseconds\n");
    fprintf(fp, "- Bytes column shows total data transferred during test\n");

    fclose(fp);
    printf("Results saved to: doc/benchmarks/%s/%s.md\n", basePlatform, reportName);
}

/*
    Archive previous latest.* files to latest-DATE.* before overwriting
 */
static void archivePreviousLatest(cchar *basePlatform)
{
    char      srcJson[256], srcMd[256], dstJson[256], dstMd[256], dateStr[32];
    time_t    now;
    struct tm *tm_info;
    FILE      *srcFp;

    // Only archive if we're saving to "latest"
    if (!smatch(reportName, "latest")) {
        return;
    }
    // Don't archive if running a subset of tests (TESTME_CLASS defined)
    if (getenv("TESTME_CLASS") && *getenv("TESTME_CLASS")) {
        return;
    }

    // Check if latest.json5 exists
    snprintf(srcJson, sizeof(srcJson), "../../doc/benchmarks/%s/latest.json5", basePlatform);
    srcFp = fopen(srcJson, "r");
    if (!srcFp) {
        return;  // No previous latest to archive
    }
    fclose(srcFp);

    // Generate date-time string (YYYY-MM-DD-HHMM)
    time(&now);
    tm_info = localtime(&now);
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d-%H%M", tm_info);

    // Build destination paths
    snprintf(srcMd, sizeof(srcMd), "../../doc/benchmarks/%s/latest.md", basePlatform);
    snprintf(dstJson, sizeof(dstJson), "../../doc/benchmarks/%s/latest-%s.json5", basePlatform, dateStr);
    snprintf(dstMd, sizeof(dstMd), "../../doc/benchmarks/%s/latest-%s.md", basePlatform, dateStr);

    // Copy latest.json5 to latest-DATE.json5
    if (rename(srcJson, dstJson) == 0) {
        printf("Archived previous results to: doc/benchmarks/%s/latest-%s.json5\n", basePlatform, dateStr);
    }

    // Copy latest.md to latest-DATE.md
    if (rename(srcMd, dstMd) == 0) {
        printf("Archived previous results to: doc/benchmarks/%s/latest-%s.md\n", basePlatform, dateStr);
    }
}

void saveFinalResults(void)
{
    Json      *root, *config;
    char      *platform, *profile, *output, *osver, *machine, platformInfo[256];
    char      timestamp[64], basePlatform[64], path[256];
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
    strftime(timestamp, sizeof(timestamp), "%b %d, %Y %I:%M %p UTC", tm_info);
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
    // Extract base platform for directory (e.g., "macosx-arm64" -> "macosx")
    scopy(basePlatform, sizeof(basePlatform), platform);
    if (schr(basePlatform, '-')) {
        *schr(basePlatform, '-') = '\0';
    }
    // Create platform-specific benchmark directory
    snprintf(path, sizeof(path), "../../doc/benchmarks/%s", basePlatform);
    mkdir(path, 0755);

    // Archive previous latest results before overwriting
    archivePreviousLatest(basePlatform);

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
#elif WINDOWS
    {
        SYSTEM_INFO si;

        osver = rAlloc(128);
        scopy(osver, 128, "Windows");

        GetNativeSystemInfo(&si);
        machine = rAlloc(64);
        switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            scopy(machine, 64, "x86_64");
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            scopy(machine, 64, "arm64");
            break;
        default:
            scopy(machine, 64, "x86");
            break;
        }
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
        snprintf(path, sizeof(path), "../../doc/benchmarks/%s/%s.json5", basePlatform, reportName);
        fp = fopen(path, "w");
        if (fp) {
            fprintf(fp, "%s\n", output);
            fclose(fp);
            printf("\nResults saved to: doc/benchmarks/%s/%s.json5\n", basePlatform, reportName);
        } else {
            printf("Warning: Could not open doc/benchmarks/%s/%s.json5 for writing\n", basePlatform, reportName);
            printf("Results:\n%s\n", output);
        }
        rFree(output);
    }

    // Save to markdown file
    saveMarkdownResults("1.0.0-dev", timestamp, platformInfo, profile, "openssl", basePlatform);

    jsonFree(root);
}

/*
    Result Management Functions
 */

BenchResult *initResult(cchar *name, bool soak, cchar *description)
{
    if (!soak && description) {
        tinfo("%s", description);
    }
    return soak ? NULL : createBenchResult(name);
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
            bctx->errors++;
            if (bctx->stopOnErrors) {
                bctx->fatal = true;
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
    Setup HTTP and HTTPS endpoints from web.json5 configuration
 */
bool benchSetup(char **HTTP, char **HTTPS)
{
    Json *json;

    if (HTTP || HTTPS) {
        json = jsonParseFile("web.json5", NULL, 0);
        if (!json) {
            printf("Cannot parse web.json5\n");
            return 0;
        }
        if (HTTP) {
            *HTTP = jsonGetClone(json, 0, "web.listen[0]", NULL);
            if (!*HTTP) {
                printf("Cannot get HTTP endpoint\n");
                jsonFree(json);
                return 0;
            }
        }
        if (HTTPS) {
            *HTTPS = jsonGetClone(json, 0, "web.listen[1]", NULL);
            if (!*HTTPS) {
                printf("Cannot get HTTPS endpoint\n");
                jsonFree(json);
                return 0;
            }
        }
        jsonFree(json);
    }
    return 1;
}

/*
    Get the current count of TIME_WAIT sockets
    Uses netstat to count TIME_WAIT sockets on the specified port
    @param port Port number to check (0 for all ports)
    @return Number of TIME_WAIT sockets
 */
int getTimeWaits(int port)
{
    FILE *fp;
    char cmd[256], line[256];
    int  count;

    count = 0;
    if (port > 0) {
        snprintf(cmd, sizeof(cmd), "netstat -an 2>/dev/null | grep ':%d.*TIME_WAIT' 2>/dev/null | wc -l", port);
    } else {
        snprintf(cmd, sizeof(cmd), "netstat -an 2>/dev/null | grep 'TIME_WAIT' 2>/dev/null | wc -l");
    }
    fp = popen(cmd, "r");
    if (fp) {
        if (fgets(line, sizeof(line), fp)) {
            count = atoi(line);
        }
        pclose(fp);
    }
    return count;
}

/*
    Wait for TIME_WAIT sockets to drain below threshold
    @param port Port number to check (0 for all ports)
    @param maxWaits Maximum TIME_WAIT count threshold (0 for default BENCH_MAX_TIME_WAITS)
 */
void waitForTimeWaits(int port, int maxWaits)
{
    int count, waited, threshold;

    threshold = maxWaits > 0 ? maxWaits : BENCH_MAX_TIME_WAITS;
    waited = 0;
    while (1) {
        count = getTimeWaits(port);
        if (count < threshold) {
            if (waited) {
                printf("Time waits drained: %d, continuing\n\n", count);
                fflush(stdout);
            }
            break;
        }
        if (!waited) {
            printf("\nDraining TIME_WAIT sockets (current: %d, max: %d)...\n", count, threshold);
            fflush(stdout);
        }
        waited = 1;
        rSleep(1000);
    }
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
