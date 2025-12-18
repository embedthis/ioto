/*
    bench.tst.c - Web server performance benchmark suite

    Measures throughput, latency, and performance characteristics
    for regression testing across releases.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

// Includes

#include "bench-test.h"
#include "bench-utils.h"
#include "bench-utils.c"

// Locals

// Benchmark timing constants
#define URL_TIMEOUT_MS   10000   // 10 second timeout to prevent hangs

#define NUM_SOAK_GROUPS  9
#define NUM_BENCH_GROUPS 12

/*
    List of all benchmark classes in run order
 */
static cchar *benchClasses[] = {
    "throughput", "static", "https", "raw_http", "raw_https",
    "websockets", "put", "upload", "auth", "actions", "mixed", "connections",
    NULL
};

/*
    List of benchmark classes for soak phase (excludes throughput and raw_* tests)
 */
static cchar *soakClasses[] = {
    "static", "https", "websockets", "put", "upload", "auth", "actions", "mixed", "connections",
    NULL
};

static char *HTTP;
static char *HTTPS;

// Global benchmark context (shared across all benchmark functions)
static BenchContext benchCtx;
BenchContext        *bctx = &benchCtx;

// Forward declarations for benchmark functions
static void benchStaticFiles(Ticks duration);
static void benchStaticFilesRaw(Ticks duration, cchar *host, int port, bool useTls);
static void benchHTTPS(Ticks duration);
static void benchPut(Ticks duration);
static void benchUpload(Ticks duration);
static void benchAuth(Ticks duration);
static void benchActions(Ticks duration);
static void benchMixed(Ticks duration);
static void benchWebSockets(Ticks duration);
static void benchConnections(Ticks duration, cchar *host, int port, bool useTls, bool useSession, int resultIndex);
static void testWrk(void);
static void fiberMain(void *data);
static cchar *initBench(void);
static void runSoakTest(cchar *classes[], int numClasses, Ticks duration);
static bool runBenchList(cchar *classes[], Ticks perGroupDuration, bool record);
static void benchTrace(cchar *fmt, ...);
static void parseEndpoint(cchar *endpoint, cchar *scheme, char **host, int *port);

/*
    Helper: Check if iteration limit reached (returns true to break)
 */
static inline bool iterLimit(int iterations, bool warm, int coldLimit)
{
    if (bctx->soak && iterations > BENCH_MAX_SOAK_ITERATIONS) return true;
    if (!warm && iterations > coldLimit) return true;
    return false;
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
    return (bctx->fatal || bctx->errors > 0) ? 1 : 0;
}

/*
   Fiber main function - runs all benchmarks
 */
static void fiberMain(void *data)
{
    cchar *testClass;

    testClass = initBench();
    if (testClass == (cchar*) -1) {
        // Skip requested (TESTME_DURATION=0)
        goto done;
    }
    if (bctx->fatal) {
        goto done;
    }

    // Phase 1: Soak (warmup)
    cchar *singleClass[] = { testClass, NULL };
    cchar **classes = testClass ? singleClass : soakClasses;
    int   numClasses = testClass ? 1 : NUM_SOAK_GROUPS;
    runSoakTest(classes, numClasses, getSoakDuration());

    // Phase 2: Benchmark (measurement)
    if (!bctx->fatal) {
        runBenchList(testClass ? singleClass : benchClasses, getBenchDuration(), true);
    }
    // Phase 3: Save results
    if (!bctx->fatal) {
        tinfo("=== Phase 3: Analysis ===");
        recordFinalMemory();
        saveFinalResults();
    }

done:
    printf("\n");
    printf("=========================================\n");
    if (bctx->errors > 0) {
        printf("BENCHMARK RESULT: FAILED (%d errors)\n", bctx->errors);
        printf("=========================================\n");
        ttrue(false, "Benchmark suite completed with %d total errors", bctx->errors);
    } else {
        printf("BENCHMARK RESULT: PASSED (no errors)\n");
        printf("=========================================\n");
        ttrue(true, "Benchmark suite completed successfully with no errors");
    }
    fflush(stdout);
    rFree(HTTP);
    rFree(HTTPS);
    rStop();
}

/*
    Run a single benchmark class
    Waits for TIME_WAIT sockets to drain before running
    Returns false if fatal error occurred
 */
static bool runBenchClass(cchar *testClass, Ticks duration)
{
    char *host, *httpsHost;
    int  httpPort, httpsPort;

    // Check for fatal error before running
    if (bctx->fatal) {
        return false;
    }
    // Wait for TIME_WAIT sockets to drain before running
    waitForTimeWaits(0, 0);

    if (smatch(testClass, "static")) {
        benchStaticFiles(duration);

    } else if (smatch(testClass, "https")) {
        benchHTTPS(duration);

    } else if (smatch(testClass, "raw_http")) {
        parseEndpoint(HTTP, "http://", &host, &httpPort);
        benchStaticFilesRaw(duration, host, httpPort, false);
        rFree(host);

    } else if (smatch(testClass, "raw_https")) {
        parseEndpoint(HTTPS, "https://", &host, &httpsPort);
        benchStaticFilesRaw(duration, host, httpsPort, true);
        rFree(host);

    } else if (smatch(testClass, "put")) {
        benchPut(duration);

    } else if (smatch(testClass, "upload")) {
        benchUpload(duration);

    } else if (smatch(testClass, "auth")) {
        benchAuth(duration);

    } else if (smatch(testClass, "actions")) {
        benchActions(duration);

    } else if (smatch(testClass, "mixed")) {
        benchMixed(duration);

    } else if (smatch(testClass, "websockets")) {
        benchWebSockets(duration);

    } else if (smatch(testClass, "connections")) {
        parseEndpoint(HTTP, "http://", &host, &httpPort);
        parseEndpoint(HTTPS, "https://", &httpsHost, &httpsPort);
        initBenchContext(bctx, "Connections", !bctx->soak ? "Benchmarking connections..." : NULL);
        benchConnections(duration / 3, host, httpPort, false, false, 0);
        if (!bctx->fatal) {
            benchConnections(duration / 3, httpsHost, httpsPort, true, false, 1);
        }
        if (!bctx->fatal) {
            benchConnections(duration / 3, httpsHost, httpsPort, true, true, 2);
        }
        if (!bctx->soak && !bctx->fatal) {
            finishBenchContext(bctx, 3, "connections");
        }
        rFree(host);
        rFree(httpsHost);

    } else if (smatch(testClass, "throughput")) {
        // throughput uses external wrk tool, only run when recording
        if (!bctx->soak) {
            testWrk();
        }
    }
    return !bctx->fatal;
}

/*
    Run benchmark classes from a list
    Runs each benchmark in sequence with TIME_WAIT checking between each
    If record is true, clears soak flag and prints Phase 2 header
    Returns false if fatal error occurred
 */
static bool runBenchList(cchar *classes[], Ticks perGroupDuration, bool record)
{
    if (record) {
        bctx->soak = false;
        if (classes[1] == NULL) {
            tinfo("=== Phase 2: Benchmark - %s (%lld secs) ===", classes[0], (long long) getBenchDuration() / TPS);
        } else {
            tinfo("=== Phase 2: Benchmarks ===");
        }
    }
    for (int i = 0; classes[i]; i++) {
        tinfo("  Running %s (%.1f secs)...", classes[i], perGroupDuration / 1000.0);
        if (!runBenchClass(classes[i], perGroupDuration)) {
            return false;
        }
    }
    return true;
}

/*
   Benchmark static file serving using raw sockets (no URL library overhead)
   Tests: 1KB, 10KB, 100KB, 1MB files using duration-based testing
   This provides the fastest possible client for accurate server benchmarking
 */
static void benchStaticFilesRaw(Ticks duration, cchar *host, int port, bool useTls)
{
    ConnectionCtx *ctx;
    RequestResult result;
    FileClass     *fc;
    Ticks         groupStart, groupDuration, startTime;
    char          desc[80], name[64], request[512];
    int           classIndex, warm, iterations, coldIterationLimit;

    SFMT(desc, "Benchmarking static files (Raw %s)...", useTls ? "HTTPS" : "HTTP");
    initBenchContext(bctx, useTls ? "Raw HTTPS" : "Raw HTTP", desc);
    setupTotalUnits(bctx, duration, true);

    // Run both warm (reuse socket) and cold (new socket each time) tests
    for (warm = 1; warm >= 0; warm--) {
        cchar *suffix = warm ? "raw_warm" : "raw_cold";
        cchar *connection = warm ? "keep-alive" : "close";
        int   resultOffset = warm ? 0 : 4;

        benchTrace("Running %s tests...", warm ? "warm" : "cold");

        // Initialize results for this test type
        for (classIndex = 0; fileClasses[classIndex].name; classIndex++) {
            fc = &fileClasses[classIndex];
            SFMT(name, "%s_%s", fc->name, suffix);
            bctx->results[resultOffset + classIndex] = !bctx->soak ? createBenchResult(name) : NULL;
        }

        // Create socket connection context
        ctx = createSocketCtx(warm, URL_TIMEOUT_MS, host, port, useTls);
        bctx->connCtx = ctx;
        bctx->resultOffset = resultOffset;

        // Run tests for each file class
        for (classIndex = 0; fileClasses[classIndex].name; classIndex++) {
            fc = &fileClasses[classIndex];
            groupDuration = getGroupDuration(fc);
            coldIterationLimit = getColdIterationLimit(fc, bctx->totalUnits);
            benchTrace("Testing %s for %.1f seconds...", fc->name, groupDuration / 1000.0);
            groupStart = rGetTicks();
            bctx->classIndex = classIndex;
            bctx->bytes = fc->size;

            // Pre-format HTTP request
            SFMT(request,
                 "GET /%s HTTP/1.1\r\n"
                 "Host: %s\r\n"
                 "Connection: %s\r\n"
                 "X-SEQ: %d\r\n"
                 "\r\n",
                 fc->file, host, connection, bctx->seq++);

            iterations = 0;
            while (rGetTicks() - groupStart < groupDuration) {
                iterations++;
                if (iterLimit(iterations, warm, coldIterationLimit)) break;
                startTime = rGetTicks();
                result = executeRawRequest(ctx, request, fc->size);
                if (!processResponse(bctx, &result, fc->file, startTime)) {
                    return;
                }
            }
            if (bctx->fatal) break;
        }
        if (bctx->fatal) break;

        // Cleanup connection context
        freeConnectionCtx(ctx);
        bctx->connCtx = NULL;

        // Wait for TIME_WAIT sockets to drain after cold tests
        if (!warm) {
            waitForTimeWaits(port, 0);
        }
    }
    finishBenchContext(bctx, 8, useTls ? "static_files_raw_https" : "static_files_raw_http");
}

/*
   Benchmark static file serving with keep-alive vs cold connections
   Tests: 1KB, 10KB, 100KB, 1MB files using duration-based testing
 */
static void benchStaticFiles(Ticks duration)
{
    FileClass     *fc;
    ConnectionCtx *ctx;
    RequestResult result;
    Ticks         startTime, groupStart, groupDuration;
    char          url[256], name[64];
    int           classIndex, warm, iterations, coldIterationLimit;

    initBenchContext(bctx, "Static file", "Benchmarking static files (URL library)...");
    setupTotalUnits(bctx, duration, true);

    // Run both warm (reuse connection) and cold (new connection each time) tests
    for (warm = 1; warm >= 0; warm--) {
        cchar *suffix = warm ? "warm" : "cold";
        int   resultOffset = warm ? 0 : 4;

        benchTrace("Running %s tests...", warm ? "warm" : "cold");
        // Initialize results for this test type
        for (classIndex = 0; fileClasses[classIndex].name; classIndex++) {
            fc = &fileClasses[classIndex];
            SFMT(name, "%s_%s", fc->name, suffix);
            bctx->results[resultOffset + classIndex] = !bctx->soak ? createBenchResult(name) : NULL;
        }

        // Create connection context
        ctx = createConnectionCtx(warm, URL_TIMEOUT_MS);
        bctx->connCtx = ctx;
        bctx->resultOffset = resultOffset;

        // Run tests for each file class
        for (classIndex = 0; fileClasses[classIndex].name; classIndex++) {
            fc = &fileClasses[classIndex];
            groupDuration = getGroupDuration(fc);
            coldIterationLimit = getColdIterationLimit(fc, bctx->totalUnits);
            benchTrace("Testing %s for %.1f seconds...", fc->name, groupDuration / 1000.0);
            groupStart = rGetTicks();
            bctx->classIndex = classIndex;
            bctx->bytes = fc->size;

            iterations = 0;
            while (rGetTicks() - groupStart < groupDuration) {
                iterations++;
                if (iterLimit(iterations, warm, coldIterationLimit)) break;
                startTime = rGetTicks();
                result = executeRequest(ctx, "GET", SFMT(url, "%s/%s", HTTP, fc->file), NULL, 0, NULL);
                if (!processResponse(bctx, &result, url, startTime)) {
                    return;
                }
            }
            if (bctx->fatal) break;
        }
        if (bctx->fatal) break;

        // Cleanup connection context
        freeConnectionCtx(ctx);
        bctx->connCtx = NULL;

        // Wait for TIME_WAIT sockets to drain after cold tests
        if (!warm) {
            waitForTimeWaits(0, 0);
        }
    }
    finishBenchContext(bctx, 8, "static_files");
}

/*
   Benchmark PUT requests with keep-alive vs cold connections
   Tests: 1KB, 10KB, 100KB, 1MB files using PUT with duration-based testing
 */
static void benchPut(Ticks duration)
{
    ConnectionCtx *ctx;
    RequestResult result;
    Ticks         startTime, groupStart, groupDuration;
    FileClass     *fc;
    ssize         fileSizes[4];
    char          path[256], url[256], name[64], headers[128], *fileDataArray[4];
    int           classIndex, warm, counter, numClasses, iterations, coldIterationLimit;

    initBenchContext(bctx, "PUT upload", "Benchmarking PUT uploads...");
    setupTotalUnits(bctx, duration, true);

    // Pre-read all test files before timing loops
    numClasses = 0;
    for (classIndex = 0; fileClasses[classIndex].name; classIndex++) {
        fc = &fileClasses[classIndex];
        fileDataArray[classIndex] = rReadFile(SFMT(path, "site/%s", fc->file), (size_t*) &fileSizes[classIndex]);
        if (!fileDataArray[classIndex]) {
            tinfo("Warning: Cannot read %s", fc->file);
            fileSizes[classIndex] = 0;
        }
        numClasses++;
    }

    // Run both warm (reuse connection) and cold (new connection each time) tests
    for (warm = 1; warm >= 0; warm--) {
        cchar *suffix = warm ? "warm" : "cold";
        int   resultOffset = warm ? 0 : 4;

        benchTrace("Running %s tests...", warm ? "warm" : "cold");
        // Initialize results for this test type
        for (classIndex = 0; fileClasses[classIndex].name; classIndex++) {
            fc = &fileClasses[classIndex];
            SFMT(name, "%s_%s", fc->name, suffix);
            bctx->results[resultOffset + classIndex] = !bctx->soak ? createBenchResult(name) : NULL;
        }
        // Create connection context
        ctx = createConnectionCtx(warm, URL_TIMEOUT_MS);
        bctx->connCtx = ctx;
        bctx->resultOffset = resultOffset;

        // Run tests for each file class
        for (classIndex = 0; fileClasses[classIndex].name; classIndex++) {
            fc = &fileClasses[classIndex];
            if (!fileDataArray[classIndex]) {
                continue;
            }
            groupDuration = getGroupDuration(fc);
            coldIterationLimit = getColdIterationLimit(fc, bctx->totalUnits);

            benchTrace("Testing %s for %.1f seconds...", fc->name, groupDuration / 1000.0);

            counter = 0;
            bctx->classIndex = classIndex;
            bctx->bytes = fileSizes[classIndex];
            groupStart = rGetTicks();

            iterations = 0;
            while (rGetTicks() - groupStart < groupDuration) {
                iterations++;
                if (iterLimit(iterations, warm, coldIterationLimit)) break;
                SFMT(url, "%s/put/bench-%d-%d.txt", HTTP, getpid(), counter);
                SFMT(headers, "X-Sequence: %d\r\n", bctx->seq++);
                startTime = rGetTicks();
                result = executeRequest(ctx, "PUT", url, fileDataArray[classIndex], (size_t) fileSizes[classIndex],
                                        headers);
                if (!processResponse(bctx, &result, url, startTime)) {
                    goto cleanup;
                }
                // Delete the uploaded file directly to prevent buildup
                unlink(SFMT(path, "site/put/bench-%d-%d.txt", getpid(), counter));
                counter++;
            }
            if (bctx->fatal) break;
        }
        if (bctx->fatal) break;
        // Cleanup connection context
        freeConnectionCtx(ctx);
        bctx->connCtx = NULL;

        // Wait for TIME_WAIT sockets to drain after cold tests
        if (!warm) {
            waitForTimeWaits(0, 0);
        }
    }
cleanup:
    finishBenchContext(bctx, 8, "put");

    // Free pre-read file data
    for (classIndex = 0; classIndex < numClasses; classIndex++) {
        rFree(fileDataArray[classIndex]);
    }
}

/*
   Benchmark multipart/form-data uploads with keep-alive vs cold connections
   Tests: 1KB, 10KB, 100KB, 1MB files using duration-based testing
 */
static void benchUpload(Ticks duration)
{
    ConnectionCtx *ctx;
    FileClass     *fc;
    Url           *up;
    RBuf          *buf;
    RequestResult result;
    Ticks         startTime, groupStart, groupDuration;
    ssize         fileSizes[4];
    char          url[256], filepath[256], headers[256], name[64], *boundary, *fileDataArray[4];
    int           classIndex, warm, counter, numClasses, iterations, coldIterationLimit;

    initBenchContext(bctx, "Upload", "Benchmarking uploads...");
    setupTotalUnits(bctx, duration, true);

    // Pre-read all test files before timing loops
    numClasses = 0;
    for (classIndex = 0; classIndex < 4 && fileClasses[classIndex].name; classIndex++) {
        fc = &fileClasses[classIndex];
        fileDataArray[classIndex] = rReadFile(SFMT(filepath, "site/%s", fc->file), (size_t*) &fileSizes[classIndex]);
        if (!fileDataArray[classIndex]) {
            tinfo("Warning: Cannot read %s", fc->file);
            fileSizes[classIndex] = 0;
        }
        numClasses++;
    }

    // Run both warm (reuse connection) and cold (new connection each time) tests
    for (warm = 1; warm >= 0; warm--) {
        cchar *suffix = warm ? "warm" : "cold";
        int   resultOffset = warm ? 0 : 4;

        benchTrace("Running %s tests...", warm ? "warm" : "cold");
        // Initialize results for this test type
        for (classIndex = 0; classIndex < 4 && fileClasses[classIndex].name; classIndex++) {
            fc = &fileClasses[classIndex];
            SFMT(name, "%s_%s", fc->name, suffix);
            bctx->results[resultOffset + classIndex] = !bctx->soak ? createBenchResult(name) : NULL;
        }
        // Create connection context
        ctx = createConnectionCtx(warm, URL_TIMEOUT_MS);
        bctx->connCtx = ctx;
        bctx->resultOffset = resultOffset;

        // Run tests for each file class
        for (classIndex = 0; classIndex < 4 && fileClasses[classIndex].name; classIndex++) {
            fc = &fileClasses[classIndex];
            if (!fileDataArray[classIndex]) {
                continue;
            }
            // Allocate time proportionally based on multiplier
            groupDuration = (Ticks) (bctx->duration * fc->multiplier / bctx->totalUnits);
            if (groupDuration < MIN_GROUP_DURATION_MS) {
                groupDuration = MIN_GROUP_DURATION_MS;
            }
            coldIterationLimit = getColdIterationLimit(fc, bctx->totalUnits);
            benchTrace("Testing %s for %.1f seconds...", fc->name, groupDuration / 1000.0);

            counter = 0;
            boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
            bctx->classIndex = classIndex;
            bctx->bytes = fileSizes[classIndex];
            groupStart = rGetTicks();

            for (iterations = 0; rGetTicks() - groupStart < groupDuration; ) {
                iterations++;
                if (iterLimit(iterations, warm, coldIterationLimit)) break;
                // Build multipart/form-data request
                buf = rAllocBuf((size_t) (fileSizes[classIndex] + 1024));
                rPutToBuf(buf,
                          "--%s\r\n"
                          "Content-Disposition: form-data; name=\"description\"\r\n"
                          "\r\n"
                          "benchmark upload\r\n"
                          "--%s\r\n"
                          "Content-Disposition: form-data; name=\"file\"; filename=\"bench-mp-%d-%d.txt\"\r\n"
                          "Content-Type: text/plain\r\n"
                          "\r\n",
                          boundary, boundary, getpid(), counter);
                rPutBlockToBuf(buf, fileDataArray[classIndex], (size_t) fileSizes[classIndex]);
                rPutToBuf(buf, "\r\n--%s--\r\n", boundary);

                SFMT(headers,
                     "Content-Type: multipart/form-data; boundary=%s\r\nX-Sequence: %d\r\n",
                     boundary, bctx->seq++);

                // Upload file
                up = getConnection(ctx);
                startTime = rGetTicks();
                SFMT(url, "%s/test/bench/", HTTP);
                result.status = urlFetch(up, "POST", url, rGetBufStart(buf), rGetBufLength(buf), headers);
                getResponse(up);
                rFreeBuf(buf);

                if (!processResponse(bctx, &result, url, startTime)) {
                    // Note: processResponse already freed ctx via bctx->connCtx when returning false
                    goto cleanup;
                }

                // Delete uploaded file immediately to avoid accumulation
                if (result.success) {
                    char tmppath[128];
                    unlink(SFMT(tmppath, "tmp/bench-mp-%d-%d.txt", getpid(), counter));
                }
                releaseConnection(ctx);
                counter++;
            }
            if (bctx->fatal) break;
        }
        if (bctx->fatal) break;

        // Cleanup connection context
        freeConnectionCtx(ctx);
        bctx->connCtx = NULL;

        // Wait for TIME_WAIT sockets to drain after cold tests
        if (!warm) {
            waitForTimeWaits(0, 0);
        }
    }

cleanup:
    finishBenchContext(bctx, 8, "upload");

    // Free pre-read file data
    for (classIndex = 0; classIndex < numClasses; classIndex++) {
        rFree(fileDataArray[classIndex]);
    }
}

/*
   Benchmark action handlers
   Tests: Simple action, JSON action with warm/cold connections using duration-based testing
 */
static void benchActions(Ticks duration)
{
    ConnectionCtx *ctx;
    RequestResult result;
    Ticks         startTime, groupStart, groupDuration;
    char          url[256], name[64];
    int           warm, actionIndex, resultOffset, iterations;
    cchar         *suffix;

    // Used in URLs
    cchar *actions[] = { "bench", NULL };

    // Used in results
    cchar *actionNames[] = { "simple" };

    initBenchContext(bctx, "Action", "Benchmarking action handlers...");

    // Run both warm (reuse connection) and cold (new connection each time) tests
    for (warm = 1; warm >= 0; warm--) {
        suffix = warm ? "warm" : "cold";
        resultOffset = warm ? 0 : 1;

        benchTrace("Running %s tests...", warm ? "warm" : "cold");

        // Initialize results for both actions
        for (actionIndex = 0; actions[actionIndex]; actionIndex++) {
            SFMT(name, "%s_%s", actionNames[actionIndex], suffix);
            bctx->results[resultOffset + actionIndex] = initResult(name, bctx->soak, NULL);
        }
        // Create connection context
        ctx = createConnectionCtx(warm, URL_TIMEOUT_MS);
        bctx->connCtx = ctx;
        bctx->resultOffset = resultOffset;

        // Test each action
        for (actionIndex = 0; actions[actionIndex]; actionIndex++) {
            // Allocate time equally across all test cases (2 total: 1 action × 2 warm/cold)
            groupDuration = calcEqualDuration(duration, 2);

            benchTrace("Testing %s for %.1f seconds...", actionNames[actionIndex], groupDuration / 1000.0);

            groupStart = rGetTicks();
            bctx->classIndex = actionIndex;

            iterations = 0;
            while (rGetTicks() - groupStart < groupDuration) {
                iterations++;
                if (iterLimit(iterations, warm, BENCH_MAX_COLD_ITERATIONS)) break;
                startTime = rGetTicks();
                result = executeRequest(ctx, "GET", SFMT(url, "%s/test/%s/", HTTP, actions[actionIndex]), NULL, 0,
                                        NULL);
                bctx->bytes = result.bytes;
                if (!processResponse(bctx, &result, url, startTime)) {
                    return;
                }
            }
            if (bctx->fatal) break;
        }
        if (bctx->fatal) break;

        // Cleanup connection context
        freeConnectionCtx(ctx);
        bctx->connCtx = NULL;

        // Wait for TIME_WAIT sockets to drain after cold tests
        if (!warm) {
            waitForTimeWaits(0, 0);
        }
    }
    finishBenchContext(bctx, 2, "actions");
}

/*
   Benchmark authenticated routes with digest authentication
   Tests: Digest auth with session reuse, cold auth using duration-based testing
 */
static void benchAuth(Ticks duration)
{
    ConnectionCtx *ctx;
    RequestResult result;
    Url           *up;
    Ticks         startTime, groupStart;
    char          url[256], desc[80];
    int           warm, iterations;

    initBenchContext(bctx, "Auth", "Benchmarking digest authentication...");

    // Run both warm (reuse connection) and cold (new connection each time) tests
    for (warm = 1; warm >= 0; warm--) {
        cchar *name = warm ? "digest_with_session" : "digest_cold";

        // Initialize result for this test type
        SFMT(desc, "  Running %s tests for %.1f seconds...", warm ? "warm" : "cold", (duration / 2) / 1000.0);
        bctx->results[!warm] = initResult(name, bctx->soak, desc);

        // Create connection context
        ctx = createConnectionCtx(warm, URL_TIMEOUT_MS);
        bctx->connCtx = ctx;
        bctx->classIndex = !warm;

        groupStart = rGetTicks();
        iterations = 0;
        while (rGetTicks() - groupStart < duration / 2) {
            // Cap auth iterations to avoid session limit issues on some platforms
            int authLimit = BENCH_MAX_AUTH_ITERATIONS / 2;
            int coldLimit = MIN(BENCH_MAX_COLD_ITERATIONS, authLimit);
            iterations++;
            if (iterLimit(iterations, warm, coldLimit)) break;
            if (warm && iterations > authLimit) break;
            // Get connection from context
            up = getConnection(ctx);
            urlSetAuth(up, "bench", "password", "digest");

            startTime = rGetTicks();
            result.status = urlFetch(up, "GET", SFMT(url, "%s/auth/secret.html", HTTP), NULL, 0, NULL);
            getResponse(up);
            bctx->bytes = up->rxLen;
            releaseConnection(ctx);

            if (!processResponse(bctx, &result, url, startTime)) {
                return;
            }
        }
        if (bctx->fatal) break;

        // Cleanup connection context
        freeConnectionCtx(ctx);
        bctx->connCtx = NULL;

        // Wait for TIME_WAIT sockets to drain after cold tests
        if (!warm) {
            waitForTimeWaits(0, 0);
        }
    }
    finishBenchContext(bctx, 2, "auth");
}

/*
   Benchmark HTTPS performance
   Tests: 1KB, 10KB, 100KB, 1MB files with TLS handshakes and session reuse
 */
static void benchHTTPS(Ticks duration)
{
    FileClass     *fc;
    ConnectionCtx *ctx;
    RequestResult result;
    Ticks         startTime, groupStart, groupDuration;
    char          url[256], name[64];
    int           classIndex, warm, iterations, coldIterationLimit;

    initBenchContext(bctx, "HTTPS", "Benchmarking HTTPS (URL library)...");
    setupTotalUnits(bctx, duration, true);

    // Run both warm (reuse connection) and cold (new connection each time) tests
    for (warm = 1; warm >= 0; warm--) {
        cchar *suffix = warm ? "warm" : "cold";
        int   resultOffset = warm ? 0 : 4;

        benchTrace("Running %s tests...", warm ? "warm" : "cold");

        // Initialize results for this test type
        for (classIndex = 0; fileClasses[classIndex].name; classIndex++) {
            fc = &fileClasses[classIndex];
            SFMT(name, "%s_%s", fc->name, suffix);
            bctx->results[resultOffset + classIndex] = !bctx->soak ? createBenchResult(name) : NULL;
        }

        // Create connection context
        ctx = createConnectionCtx(warm, URL_TIMEOUT_MS);
        bctx->connCtx = ctx;
        bctx->resultOffset = resultOffset;

        // Run tests for each file class
        for (classIndex = 0; fileClasses[classIndex].name; classIndex++) {
            fc = &fileClasses[classIndex];
            // Allocate time proportionally based on multiplier
            groupDuration = (Ticks) (bctx->duration * fc->multiplier / bctx->totalUnits);
            if (groupDuration < MIN_GROUP_DURATION_MS) {
                groupDuration = MIN_GROUP_DURATION_MS;
            }
            coldIterationLimit = getColdIterationLimit(fc, bctx->totalUnits);

            benchTrace("Testing %s for %.1f seconds...", fc->name, groupDuration / 1000.0);

            bctx->classIndex = classIndex;
            bctx->bytes = fc->size;
            groupStart = rGetTicks();

            iterations = 0;
            while (rGetTicks() - groupStart < groupDuration) {
                iterations++;
                if (iterLimit(iterations, warm, coldIterationLimit)) break;
                startTime = rGetTicks();
                result = executeRequest(ctx, "GET", SFMT(url, "%s/%s", HTTPS, fc->file), NULL, 0, NULL);
                if (!processResponse(bctx, &result, url, startTime)) {
                    return;
                }
            }
            if (bctx->fatal) break;
        }
        if (bctx->fatal) break;

        // Cleanup connection context
        freeConnectionCtx(ctx);
        bctx->connCtx = NULL;

        // Wait for TIME_WAIT sockets to drain after cold tests
        if (!warm) {
            waitForTimeWaits(0, 0);
        }
    }
    finishBenchContext(bctx, 8, "https");
}

/*
   WebSocket benchmark data
 */
typedef struct {
    BenchResult *result;
    Ticks startTime;
    int messagesRemaining;
    RFiber *fiber;
} WebSocketBenchData;

/*
   WebSocket callback for benchmark - tracks roundtrip time
   Sends initial message on open and then sends "messagesRemaining" messages.
   When complete, resumes the fiber and closes the connection.
 */
static void webSocketBenchCallback(WebSocket *ws, int event, cchar *data, size_t len, void *arg)
{
    WebSocketBenchData *benchData = (WebSocketBenchData*) arg;
    Ticks              elapsed;

    if (event == WS_EVENT_OPEN) {
        // Connection established - send first message
        benchData->startTime = rGetTicks();
        webSocketSend(ws, "Benchmark message %d", benchData->messagesRemaining);
        benchData->messagesRemaining--;

    } else if (event == WS_EVENT_MESSAGE) {
        // Message echoed back - record timing
        elapsed = rGetTicks() - benchData->startTime;
        if (benchData->result) {
            recordRequest(benchData->result, true, elapsed, (ssize) len);
        }
        // Send next message if we have more to send
        if (benchData->messagesRemaining > 0) {
            benchData->startTime = rGetTicks();
            webSocketSend(ws, "Benchmark message %d", benchData->messagesRemaining);
            benchData->messagesRemaining--;
        } else {
            // Done - send close message (fiber will resume on WS_EVENT_CLOSE)
            webSocketSendClose(ws, WS_STATUS_OK, "Benchmark complete");
        }

    } else if (event == WS_EVENT_CLOSE || event == WS_EVENT_ERROR) {
    }
}

/*
   Benchmark WebSocket operations
   Tests: Message roundtrip time with echo server
 */
static void benchWebSockets(Ticks duration)
{
    RequestResult      result;
    WebSocketBenchData benchData;
    ConnectionCtx      *ctx;
    char               *url, ubuf[80], desc[80];
    Ticks              startTime, reqStart;
    int                iterations;

    /*
       if (smatch(getenv("TESTME_REPORT"), "appweb")) {
        tinfo("Skipping web sockets benchmark for appweb");
        return;
       } */
    initBenchContext(bctx, "WebSocket", "Benchmarking WebSockets...");
    SFMT(desc, "  Running echo tests for %.1f seconds...", duration / 1000.0);
    bctx->results[0] = initResult("websocket_echo", bctx->soak, desc);
    startTime = rGetTicks();

    // WebSockets always use cold connections (new connection per upgrade)
    ctx = createConnectionCtx(false, URL_TIMEOUT_MS);
    bctx->connCtx = ctx;
    bctx->bytes = 0;

    iterations = 0;
    while (rGetTicks() - startTime < duration) {
        iterations++;
        if (iterLimit(iterations, false, BENCH_MAX_COLD_ITERATIONS)) break;
        // Prepare benchmark data
        benchData.messagesRemaining = 1000;  // Send 1000 messages per connection
        benchData.result = bctx->results[0];
        benchData.startTime = 0;
        benchData.fiber = rGetFiber();

        // Create new WebSocket connection for each batch
        url = sreplace(SFMT(ubuf, "%s/test/ws/", HTTP), "http", "ws");

        reqStart = rGetTicks();
        result.status = urlWebSocket(url, (WebSocketProc) webSocketBenchCallback, &benchData, NULL);
        if (result.status == 0) {
            // Pretend it is a success
            result.status = 200;
        }
        if (!processResponse(bctx, &result, url, reqStart)) {
            rFree(url);
            ttrue(false, "TESTME_STOP: Stopping benchmark due to WebSocket error");
            return;
        }
        rFree(url);
    }
    freeConnectionCtx(ctx);
    waitForTimeWaits(0, 0);
    finishBenchContext(bctx, 1, "websockets");
}

/*
   Benchmark connection establishment only (no HTTP request)
   Tests raw TCP and TLS handshake overhead
   Always cold - new connection each time

   Test types (resultIndex):
   - 0: useTls=false, useSession=false: Plain TCP connection
   - 1: useTls=true, useSession=false: TLS cold (full handshake each time)
   - 2: useTls=true, useSession=true: TLS with session caching (session resumption)
 */
static void benchConnections(Ticks duration, cchar *host, int port, bool useTls, bool useSession, int resultIndex)
{
    RSocket *sp;
    Ticks   startTime, groupStart, elapsed;
    char    desc[80], name[32];
    void    *cachedSession;
    int     status, iterations;

    // Determine test name based on mode
    if (!useTls) {
        scopy(name, sizeof(name), "plain");
        SFMT(desc, "  Running plain TCP connections for %.1f seconds...", duration / 1000.0);
    } else if (useSession) {
        scopy(name, sizeof(name), "tls_session");
        SFMT(desc, "  Running TLS (session) connections for %.1f seconds...", duration / 1000.0);
    } else {
        scopy(name, sizeof(name), "tls_cold");
        SFMT(desc, "  Running TLS (cold) connections for %.1f seconds...", duration / 1000.0);
    }
    bctx->results[resultIndex] = initResult(name, bctx->soak, desc);
    bctx->bytes = 0;
    cachedSession = NULL;

    // For session caching mode, establish initial connection to get a session
    if (useTls && useSession) {
        sp = rAllocSocket();
        if (sp) {
            // sp->flags |= R_SOCKET_FAST_CONNECT | R_SOCKET_FAST_CLOSE;
            rSetTls(sp);
            // rSetSocketLinger(sp, 0);
            status = rConnectSocket(sp, host, port, rGetTicks() + URL_TIMEOUT_MS);
            if (status >= 0) {
                cachedSession = rGetTlsSession(sp);
            }
            rFreeSocket(sp);
        }
        if (!cachedSession) {
            tfail("Could not establish initial TLS session for caching");
            return;
        }
    }

    groupStart = rGetTicks();
    iterations = 0;
    while (rGetTicks() - groupStart < duration) {
        iterations++;
        if (iterLimit(iterations, false, BENCH_MAX_COLD_ITERATIONS)) break;
        startTime = rGetTicks();

        // Create socket
        sp = rAllocSocket();
        if (!sp) {
            bctx->errorCount++;
            bctx->errors++;
            continue;
        }
        // Fast mode: skip connect verification and linger drain for benchmark
        // sp->flags |= R_SOCKET_FAST_CONNECT | R_SOCKET_FAST_CLOSE;

        // Enable TLS mode before connecting (handshake happens during connect)
        if (useTls) {
            rSetTls(sp);
            // Apply cached session for resumption
            if (useSession && cachedSession) {
                rSetTlsSession(sp, cachedSession);
            }
        }
        // rSetSocketLinger(sp, 0);

        // Connect (includes TLS handshake if TLS is enabled)
        status = rConnectSocket(sp, host, port, rGetTicks() + URL_TIMEOUT_MS);
        if (status < 0) {
            rFreeSocket(sp);
            bctx->errorCount++;
            bctx->errors++;
            if (bctx->stopOnErrors) {
                bctx->fatal = true;
                break;
            }
            continue;
        }
        // Update cached session if session caching is enabled
        if (useTls && useSession) {
            void *newSession = rGetTlsSession(sp);
            if (newSession) {
                if (cachedSession) {
                    rFreeTlsSession(cachedSession);
                }
                cachedSession = newSession;
            }
        }
        // Close immediately - no request sent
        rFreeSocket(sp);

        elapsed = rGetTicks() - startTime;
        bctx->totalRequests++;
        if (bctx->results[resultIndex]) {
            recordRequest(bctx->results[resultIndex], true, elapsed, 0);
        }
    }
    // Free cached session
    if (cachedSession) {
        rFreeTlsSession(cachedSession);
    }
    // Wait for TIME_WAIT sockets to drain
    waitForTimeWaits(port, 0);
}

/*
   Benchmark mixed workload - realistic traffic pattern
   70% GET requests, 20% actions, 10% uploads
 */
static void benchMixed(Ticks duration)
{
    ConnectionCtx *ctx;
    RequestResult reqResult;
    Url           *up;
    Ticks         startTime, groupStart;
    char          url[256], desc[80];
    char          *fileData;
    ssize         bodyLen;
    int           cycle, reqType;

    initBenchContext(bctx, "Mixed", "Benchmarking mixed workload...");

    SFMT(desc, "  Running mixed tests for %.1f seconds...", duration / 1000.0);
    bctx->results[0] = initResult("mixed_workload", bctx->soak, desc);
    ctx = createConnectionCtx(true, URL_TIMEOUT_MS);

    bctx->connCtx = ctx;
    up = getConnection(ctx);

    // Read test file for upload data
    fileData = rReadFile("site/static/1K.txt", (size_t*) &bodyLen);
    if (!fileData) {
        tinfo("Failed to read site/static/1K.txt");
        freeConnectionCtx(ctx);
        return;
    }

    groupStart = rGetTicks();
    cycle = 0;

    while (rGetTicks() - groupStart < duration) {
        /*
            Determine request type based on cycle (70% GET, 20% action, 10% upload)
            Pattern: G G G G G G G A A U (10 requests = 7 GET + 2 action + 1 upload)
         */
        reqType = cycle % 10;
        startTime = rGetTicks();

        if (reqType < 7) {
            // 70% - GET static file (alternate between file sizes)
            if (cycle % 4 == 0) {
                reqResult.status = urlFetch(up, "GET", SFMT(url, "%s/static/1K.txt", HTTP), NULL, 0, NULL);
            } else if (cycle % 4 == 1) {
                reqResult.status = urlFetch(up, "GET", SFMT(url, "%s/static/10K.txt", HTTP), NULL, 0, NULL);
            } else {
                reqResult.status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
            }
            urlGetResponse(up);
            bctx->bytes = up->rxLen;

        } else if (reqType < 9) {
            reqResult.status = urlFetch(up, "GET", SFMT(url, "%s/test/bench/", HTTP), NULL, 0, NULL);
            urlGetResponse(up);
            bctx->bytes = up->rxLen;

        } else {
            // 10% - Upload (PUT request)
            reqResult.status = urlFetch(up, "PUT", SFMT(url, "%s/put/bench-%d.txt", HTTP, getpid()),
                                        fileData, (size_t) bodyLen, NULL);
            urlGetResponse(up);
            bctx->bytes = bodyLen;
        }
        if (!processResponse(bctx, &reqResult, url, startTime)) {
            rFree(fileData);
            return;
        }
        cycle++;
    }
    rFree(fileData);
    freeConnectionCtx(ctx);
    finishBenchContext(bctx, 1, "mixed");
}

/*
   Inner function: Run wrk benchmark with specified parameters
   Returns BenchResult* or NULL on failure
 */
static BenchResult *runWrkInner(cchar *host, int port, int threads, int connections, int durationSecs, cchar *testName)
{
    BenchResult *result;
    double      reqPerSec, avgLatency;
    char        cmd[1024], tmpfile[256], *output, *line;
    int         rc;

    tinfo("Target: http://%s:%d/static/1K.txt", host, port);
    tinfo("Threads: %d, Connections: %d, Duration: %ds", threads, connections, durationSecs);

    // Run wrk benchmark and capture output to temp file
    SFMT(tmpfile, "wrk-bench-%d.txt", getpid());
    SFMT(cmd, "wrk -t%d -c%d -d%ds http://%s:%d/static/1K.txt 2>&1 | tee %s",
         threads, connections, durationSecs, host, port, tmpfile);
    printf("INVOKE %s\n", cmd);
    fflush(stdout);
    rc = system(cmd);

    if (rc != 0) {
        tinfo("Warning: wrk command failed with exit code %d", rc);
        return NULL;
    }

    // Read and parse wrk output
    output = rReadFile(tmpfile, NULL);
    if (!output) {
        tinfo("Warning: Could not read wrk output");
        return NULL;
    }
    // Parse key metrics from wrk output
    reqPerSec = 0;
    avgLatency = 0;

    // Look for "Requests/sec: 12345.67"
    line = scontains(output, "Requests/sec:");
    if (line) {
        reqPerSec = stof(line + 13);  // Skip "Requests/sec:"
    }

    // Look for "Latency     1.23ms" (wrk formats with whitespace padding)
    line = scontains(output, "Latency");
    if (line) {
        // Skip past "Latency" and whitespace to get to the number
        char *p = line + 7;  // Skip "Latency"
        while (*p && (*p == ' ' || *p == '\t')) p++;
        avgLatency = stof(p);
        // Check if it's in microseconds (us) or milliseconds (ms)
        if (scontains(p, "us")) {
            avgLatency /= 1000.0;  // Convert microseconds to milliseconds
        }
    }

    // Create benchmark result
    result = createBenchResult(testName);
    result->requestsPerSec = reqPerSec;
    result->avgTime = avgLatency;
    result->iterations = (int) (reqPerSec * durationSecs);
    result->totalTime = durationSecs * TPS;
    result->minTime = 0;
    result->maxTime = 0;
    result->p95Time = 0;
    result->p99Time = 0;
    result->bytesTransferred = result->iterations * 1024;  // Approximate: 1KB per request
    result->errors = 0;

    // Cleanup
    unlink(tmpfile);
    rFree(output);

    return result;
}

/*
   Test: Benchmark using wrk tool for maximum raw throughput
   Invokes external wrk benchmarking tool if available
   Runs two configurations:
   - High throughput: 12 threads, 40 connections
   - Single thread: 1 thread, 1 connection
 */
static void testWrk(void)
{
    BenchResult *results[2];
    Ticks       duration;
    char        *host, *portStr;
    int         port, rc, durationSecs, resultCount;

    // Parse HTTP endpoint for host and port
    if (!HTTP || !scontains(HTTP, "://")) {
        tinfo("Skipping wrk benchmark - invalid endpoint");
        return;
    }
    host = sclone(HTTP + 7);  // Skip "http://"
    portStr = schr(host, ':');
    if (portStr) {
        *portStr = '\0';
        port = atoi(portStr + 1);
    } else {
        port = 80;
    }

    tinfo("=== Benchmarking with wrk (Maximum Raw Throughput) ===");

#if WINDOWS
    tinfo("SKIP: wrk benchmark not available on Windows");
    rFree(host);
    return;
#endif

    // Check if wrk is available
    rc = system("command -v wrk >/dev/null 2>&1");
    if (rc != 0) {
        tinfo("SKIP: wrk not installed - install from https://github.com/wg/wrk");
        rFree(host);
        return;
    }

    // Get configured duration (convert from milliseconds to seconds, minimum 5s for meaningful wrk results)
    duration = getBenchDuration();
    durationSecs = (int) (duration / 1000);
    if (durationSecs < 5) {
        durationSecs = 5;
    }

    resultCount = 0;

    // Run high throughput configuration: 12 threads, 40 connections
    results[0] = runWrkInner(host, port, 12, 40, durationSecs, "multithread");
    if (results[0]) {
        printBenchResult(results[0]);
        resultCount++;
    }

    // Run single thread configuration: 1 thread, 1 connection
    results[1] = runWrkInner(host, port, 1, 1, durationSecs, "singlethread");
    if (results[1]) {
        printBenchResult(results[1]);
        resultCount++;
    }

    // Save both results together under throughput
    if (resultCount > 0) {
        saveBenchGroup("throughput", results, 2);
    }
    freeBenchResult(results[0]);
    freeBenchResult(results[1]);

    rFree(host);
}

/*
    Check if a test class name is valid
 */
static bool isValidBenchClass(cchar *testClass)
{
    for (int i = 0; benchClasses[i]; i++) {
        if (smatch(testClass, benchClasses[i])) {
            return true;
        }
    }
    return false;
}

/*
    Print benchmark banner
 */
static void printBanner(cchar *testClass)
{
    printf("\n");
    printf("=========================================\n");
    printf("Web Server Performance Benchmark Suite\n");
    if (testClass) {
        printf("Single Class: %s\n", testClass);
    }
    printf("=========================================\n");
    printf("HTTP:  %s\n", HTTP);
    printf("HTTPS: %s\n", HTTPS);
    printf("=========================================\n");
    printf("\n");
    fflush(stdout);
}

/*
    Initialize benchmark environment
    Returns the test class to run (NULL for all tests), or sets bctx->fatal on failure
 */
static cchar *initBench(void)
{
    cchar *testClass;

    // Set default certs and timeout for TLS connections
    rSetSocketDefaultCerts("../../certs/ca.crt", 0, 0, 0);
    urlSetDefaultTimeout(60 * TPS);

    // Get endpoints from environment
    if (getenv("TESTME_HTTP")) {
        HTTP = sclone(getenv("TESTME_HTTP"));
    }
    if (getenv("TESTME_HTTPS")) {
        HTTPS = sclone(getenv("TESTME_HTTPS"));
    }
    // Check for skip condition (not an error, just skip)
    if (smatch(getenv("TESTME_DURATION"), "0")) {
        tinfo("TESTME_DURATION is 0, skipping all tests");
        return (cchar*) -1;  // Special value to indicate skip
    }
    // Setup endpoints from web.json5 if not provided
    if (!HTTP || !HTTPS) {
        if (!benchSetup(&HTTP, &HTTPS)) {
            bctx->fatal = true;
            return NULL;
        }
    }
    if (!HTTP || !HTTPS) {
        tinfo("Error: Cannot get HTTP or HTTPS endpoints");
        bctx->fatal = true;
        return NULL;
    }
    // Check for stop-on-errors flag
    if (smatch(getenv("TESTME_STOP"), "1")) {
        bctx->stopOnErrors = true;
        tinfo("Will stop immediately on any request error");
    }
    // Check for single class mode
    testClass = getenv("TESTME_CLASS");
    if (testClass && *testClass) {
        if (!isValidBenchClass(testClass)) {
            tinfo("Error: Invalid TESTME_CLASS='%s'", testClass);
            tinfo(
                "Valid values: static, https, raw_http, raw_https, put, upload, auth, actions, mixed, websockets, connections, throughput");
            bctx->fatal = true;
            return NULL;
        }
        configureDuration(1);
        printBanner(testClass);
        return testClass;
    }
    configureDuration(NUM_BENCH_GROUPS);
    printBanner(NULL);
    return NULL;
}

/*
   Run soak test - one complete sweep of core benchmarks
   Warms up server, caches, and allows JIT optimizations
 */
static void runSoakTest(cchar *classes[], int numClasses, Ticks duration)
{
    Ticks perGroupDuration;

    perGroupDuration = duration / numClasses;

    bctx->soak = true;
    if (numClasses == 1) {
        tinfo("=== Phase 1: Soak - %s (%.1f secs) ===", classes[0], duration / 1000.0);
    } else {
        tinfo("=== Phase 1: Soak ===");
    }
    tinfo("Soak phase: Warming up all code paths...");
    runBenchList(classes, perGroupDuration, false);

    if (!bctx->fatal) {
        printf("\n");
        tinfo("Soak phase complete - all code paths warmed");
        waitForTimeWaits(0, 500);
        recordInitialMemory();
    }
}

/*
    Trace output only during benchmark phase (not soak)
 */
static void benchTrace(cchar *fmt, ...)
{
    va_list args;

    if (!bctx->soak) {
        va_start(args, fmt);
        printf("    ");
        vprintf(fmt, args);
        printf("\n");
        fflush(stdout);
        va_end(args);
    }
}

/*
    Parse HTTP/HTTPS endpoints to extract host and port
 */
static void parseEndpoint(cchar *endpoint, cchar *scheme, char **host, int *port)
{
    char *portStr;
    int  schemeLen;

    schemeLen = (int) slen(scheme);
    *host = sclone(endpoint + schemeLen);
    portStr = schr(*host, ':');
    if (portStr) {
        *portStr = '\0';
        *port = atoi(portStr + 1);
    } else {
        *port = smatch(scheme, "https://") ? 443 : 80;
    }
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
