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

static char *HTTP;
static char *HTTPS;

// Global benchmark context (shared across all benchmark functions)
static BenchContext benchCtx;
BenchContext *bctx = &benchCtx;

// Benchmark timing constants
#define URL_TIMEOUT_MS        10000     // 10 second timeout to prevent hangs
#define MIN_GROUP_DURATION_MS 500       // Minimum 500ms per test group

/*
    Iteration count configuration per file size class
    Multiplier relative to base benchmark iterations
 */
typedef struct {
    cchar *name;
    cchar *file;
    int64 size;
    double multiplier;                   // Fraction of base iterations (1.0 = full, 0.25 = 25%)
} FileClass;

static FileClass fileClasses[] = {
    { "1KB",   "static/1K.txt",   1024,       1.0  }, // Full iterations for small files
    { "10KB",  "static/10K.txt",  10240,      1.0  }, // Full iterations
    { "100KB", "static/100K.txt", 102400,     0.25 }, // 25% iterations for large files
    { "1MB",   "static/1M.txt",   1048576,    0.25 }, // 25% iterations for very large files
    { NULL,    NULL,              0,          0    }
};

/*
    Calculate group duration based on file class multiplier
    Allocates time proportionally and ensures minimum duration
 */
static Ticks getGroupDuration(FileClass *fc)
{
    Ticks duration;

    duration = (Ticks) (bctx->duration * fc->multiplier / bctx->totalUnits);
    if (duration < MIN_GROUP_DURATION_MS) {
        duration = MIN_GROUP_DURATION_MS;
    }
    return duration;
}

/*
    Calculate group duration for equal time allocation
    Divides total duration equally among groups with minimum enforcement
 */
static Ticks calcEqualDuration(Ticks duration, int numGroups)
{
    Ticks d = duration / numGroups;
    return d < MIN_GROUP_DURATION_MS ? MIN_GROUP_DURATION_MS : d;
}

/*
   Calculate total weighted units for duration allocation
   Sums all fileClasses multipliers and optionally doubles for warm/cold tests
 */
static void setupTotalUnits(BenchContext *ctx, Ticks duration, bool warmCold)
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
    int           classIndex, warm;

    SFMT(desc, "Benchmarking static files (Raw %s)...", useTls ? "HTTPS" : "HTTP");
    initBenchContext(bctx, useTls ? "Raw HTTPS" : "Raw HTTP", desc);
    setupTotalUnits(bctx, duration, true);

    // Run both warm (reuse socket) and cold (new socket each time) tests
    for (warm = 1; warm >= 0; warm--) {
        cchar *suffix = warm ? "raw_warm" : "raw_cold";
        cchar *connection = warm ? "keep-alive" : "close";
        int   resultOffset = warm ? 0 : 4;

        if (bctx->recordResults) {
            tinfo("  Running %s tests...", warm ? "warm" : "cold");
        }

        // Initialize results for this test type
        for (classIndex = 0; fileClasses[classIndex].name; classIndex++) {
            fc = &fileClasses[classIndex];
            SFMT(name, "%s_%s", fc->name, suffix);
            bctx->results[resultOffset + classIndex] = bctx->recordResults ? createBenchResult(name) : NULL;
        }

        // Create socket connection context
        ctx = createSocketCtx(warm, URL_TIMEOUT_MS, host, port, useTls);
        bctx->connCtx = ctx;
        bctx->resultOffset = resultOffset;

        // Run tests for each file class
        for (classIndex = 0; fileClasses[classIndex].name; classIndex++) {
            fc = &fileClasses[classIndex];
            groupDuration = getGroupDuration(fc);
            if (bctx->recordResults) {
                tinfo("    Testing %s for %.1f seconds...", fc->name, groupDuration / 1000.0);
            }
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

            while (rGetTicks() - groupStart < groupDuration) {
                startTime = rGetTicks();
                result = executeRawRequest(ctx, request, fc->size);
                if (!processResponse(bctx, &result, fc->file, startTime)) {
                    return;
                }
            }
            if (bctx->fatalError) break;
        }
        if (bctx->fatalError) break;

        // Cleanup connection context
        freeConnectionCtx(ctx);
        bctx->connCtx = NULL;
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
    int           classIndex, warm;

    initBenchContext(bctx, "Static file", "Benchmarking static files (URL library)...");
    setupTotalUnits(bctx, duration, true);

    // Run both warm (reuse connection) and cold (new connection each time) tests
    for (warm = 1; warm >= 0; warm--) {
        cchar *suffix = warm ? "warm" : "cold";
        int   resultOffset = warm ? 0 : 4;

        if (bctx->recordResults) {
            tinfo("  Running %s tests...", warm ? "warm" : "cold");
        }
        // Initialize results for this test type
        for (classIndex = 0; fileClasses[classIndex].name; classIndex++) {
            fc = &fileClasses[classIndex];
            SFMT(name, "%s_%s", fc->name, suffix);
            bctx->results[resultOffset + classIndex] = bctx->recordResults ? createBenchResult(name) : NULL;
        }

        // Create connection context
        ctx = createConnectionCtx(warm, URL_TIMEOUT_MS);
        bctx->connCtx = ctx;
        bctx->resultOffset = resultOffset;

        // Run tests for each file class
        for (classIndex = 0; fileClasses[classIndex].name; classIndex++) {
            fc = &fileClasses[classIndex];
            groupDuration = getGroupDuration(fc);
            if (bctx->recordResults) {
                tinfo("    Testing %s for %.1f seconds...", fc->name, groupDuration / 1000.0);
            }
            groupStart = rGetTicks();
            bctx->classIndex = classIndex;
            bctx->bytes = fc->size;

            while (rGetTicks() - groupStart < groupDuration) {
                startTime = rGetTicks();
                result = executeRequest(ctx, "GET", SFMT(url, "%s/%s", HTTP, fc->file), NULL, 0, NULL);
                if (!processResponse(bctx, &result, url, startTime)) {
                    return;
                }
            }
            if (bctx->fatalError) break;
        }
        if (bctx->fatalError) break;

        // Cleanup connection context
        freeConnectionCtx(ctx);
        bctx->connCtx = NULL;
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
    ssize         fileSize;
    char          path[256], url[256], name[64], headers[128], *fileData;
    int           classIndex, warm, counter;

    initBenchContext(bctx, "PUT upload", "Benchmarking PUT uploads...");
    setupTotalUnits(bctx, duration, true);

    // Run both warm (reuse connection) and cold (new connection each time) tests
    for (warm = 1; warm >= 0; warm--) {
        cchar *suffix = warm ? "warm" : "cold";
        int   resultOffset = warm ? 0 : 4;

        if (bctx->recordResults) {
            tinfo("  Running %s tests...", warm ? "warm" : "cold");
        }
        // Initialize results for this test type
        for (classIndex = 0; fileClasses[classIndex].name; classIndex++) {
            fc = &fileClasses[classIndex];
            SFMT(name, "%s_%s", fc->name, suffix);
            bctx->results[resultOffset + classIndex] = bctx->recordResults ? createBenchResult(name) : NULL;
        }
        // Create connection context
        ctx = createConnectionCtx(warm, URL_TIMEOUT_MS);
        bctx->connCtx = ctx;
        bctx->resultOffset = resultOffset;

        // Run tests for each file class
        for (classIndex = 0; fileClasses[classIndex].name; classIndex++) {
            fc = &fileClasses[classIndex];
            groupDuration = getGroupDuration(fc);
            if (bctx->recordResults) {
                tinfo("    Testing %s for %.1f seconds...", fc->name, groupDuration / 1000.0);
            }
            fileData = rReadFile(SFMT(path, "site/%s", fc->file), (size_t*) &fileSize);
            if (!fileData) {
                continue;
            }
            groupStart = rGetTicks();
            counter = 0;
            bctx->classIndex = classIndex;
            bctx->bytes = fileSize;

            while (rGetTicks() - groupStart < groupDuration) {
                SFMT(url, "%s/upload/bench-%d-%d.txt", HTTP, getpid(), counter);
                SFMT(headers, "X-Sequence: %d\r\n", bctx->seq++);
                startTime = rGetTicks();
                result = executeRequest(ctx, "PUT", url, fileData, (size_t) fileSize, headers);
                if (!processResponse(bctx, &result, url, startTime)) {
                    rFree(fileData);
                    return;
                }
                // Delete the uploaded file directly to prevent buildup
                unlink(SFMT(path, "site/upload/bench-%d-%d.txt", getpid(), counter));
                counter++;
            }
            if (bctx->fatalError) break;
            rFree(fileData);
        }
        if (bctx->fatalError) break;
        // Cleanup connection context
        freeConnectionCtx(ctx);
        bctx->connCtx = NULL;
    }
    finishBenchContext(bctx, 8, "put");
}

/*
   Benchmark multipart/form-data uploads with keep-alive vs cold connections
   Tests: 1KB, 10KB, 100KB, 1MB files using duration-based testing
 */
static void benchMultipartUpload(Ticks duration)
{
    ConnectionCtx *ctx;
    FileClass     *fc;
    Url           *up;
    RBuf          *buf;
    RequestResult result;
    Ticks         startTime, groupStart, groupDuration;
    char          url[256], filepath[256], headers[256], name[64], *boundary, *fileData;
    ssize         fileSize;
    int           classIndex, warm, counter;

    initBenchContext(bctx, "Multipart upload", "Benchmarking multipart uploads...");
    setupTotalUnits(bctx, duration, true);

    // Run both warm (reuse connection) and cold (new connection each time) tests
    for (warm = 1; warm >= 0; warm--) {
        cchar *suffix = warm ? "warm" : "cold";
        int   resultOffset = warm ? 0 : 4;

        if (bctx->recordResults) {
            tinfo("  Running %s tests...", warm ? "warm" : "cold");
        }
        // Initialize results for this test type
        for (classIndex = 0; classIndex < 4 && fileClasses[classIndex].name; classIndex++) {
            fc = &fileClasses[classIndex];
            SFMT(name, "%s_%s", fc->name, suffix);
            bctx->results[resultOffset + classIndex] = bctx->recordResults ? createBenchResult(name) : NULL;
        }
        // Create connection context
        ctx = createConnectionCtx(warm, URL_TIMEOUT_MS);
        bctx->connCtx = ctx;
        bctx->resultOffset = resultOffset;

        // Run tests for each file class
        for (classIndex = 0; classIndex < 4 && fileClasses[classIndex].name; classIndex++) {
            fc = &fileClasses[classIndex];

            // Allocate time proportionally based on multiplier
            groupDuration = (Ticks) (bctx->duration * fc->multiplier / bctx->totalUnits);
            if (groupDuration < MIN_GROUP_DURATION_MS) {
                groupDuration = MIN_GROUP_DURATION_MS;
            }
            if (bctx->recordResults) {
                tinfo("    Testing %s for %.1f seconds...", fc->name, groupDuration / 1000.0);
            }
            // Read file data for this size class
            fileData = rReadFile(SFMT(filepath, "site/%s", fc->file), (size_t*) &fileSize);
            if (!fileData) {
                continue;
            }
            groupStart = rGetTicks();
            counter = 0;
            boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
            bctx->classIndex = classIndex;
            bctx->bytes = fileSize;

            while (rGetTicks() - groupStart < groupDuration) {
                // Build multipart/form-data request
                buf = rAllocBuf((size_t)(fileSize + 1024));
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
                rPutBlockToBuf(buf, fileData, (size_t) fileSize);
                rPutToBuf(buf, "\r\n--%s--\r\n", boundary);

                SFMT(headers,
                     "Content-Type: multipart/form-data; boundary=%s\r\nX-Sequence: %d\r\n",
                     boundary, bctx->seq++);

                // Upload file
                up = getConnection(ctx);
                startTime = rGetTicks();
                SFMT(url, "%s/test/upload/", HTTP);
                result.status = urlFetch(up, "POST", url, rGetBufStart(buf), rGetBufLength(buf), headers);
                urlGetResponse(up);
                rFreeBuf(buf);

                if (!processResponse(bctx, &result, url, startTime)) {
                    releaseConnection(ctx);
                    rFree(fileData);
                    return;
                }

                // Delete uploaded file immediately to avoid accumulation
                if (result.success) {
                    char filepath[128];
                    unlink(SFMT(filepath, "tmp/bench-mp-%d-%d.txt", getpid(), counter));
                }
                releaseConnection(ctx);
                counter++;
            }
            if (bctx->fatalError) break;
            rFree(fileData);
        }
        if (bctx->fatalError) break;

        // Cleanup connection context
        freeConnectionCtx(ctx);
        bctx->connCtx = NULL;
    }

    finishBenchContext(bctx, 8, "multipart_upload");
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
    int           warm, actionIndex, resultOffset;
    cchar         *suffix;

    // Used in URLs
    cchar *actions[] = { "success", "show", NULL };

    // Used in results
    cchar *actionNames[] = { "simple", "json" };

    initBenchContext(bctx, "Action", "Benchmarking action handlers...");

    // Run both warm (reuse connection) and cold (new connection each time) tests
    for (warm = 1; warm >= 0; warm--) {
        suffix = warm ? "warm" : "cold";
        resultOffset = warm ? 0 : 2;

        if (bctx->recordResults) {
            tinfo("  Running %s tests...", warm ? "warm" : "cold");
        }
        // Initialize results for both actions
        for (actionIndex = 0; actions[actionIndex]; actionIndex++) {
            SFMT(name, "%s_%s", actionNames[actionIndex], suffix);
            bctx->results[resultOffset + actionIndex] = initResult(name, bctx->recordResults, NULL);
        }
        // Create connection context
        ctx = createConnectionCtx(warm, URL_TIMEOUT_MS);
        bctx->connCtx = ctx;
        bctx->resultOffset = resultOffset;

        // Test each action
        for (actionIndex = 0; actions[actionIndex]; actionIndex++) {
            // Allocate time equally across all test cases (4 total: 2 actions × 2 warm/cold)
            groupDuration = calcEqualDuration(duration, 4);
            if (bctx->recordResults) {
                tinfo("    Testing %s for %.1f seconds...", actionNames[actionIndex], groupDuration / 1000.0);
            }
            groupStart = rGetTicks();
            bctx->classIndex = actionIndex;

            while (rGetTicks() - groupStart < groupDuration) {
                startTime = rGetTicks();
                result = executeRequest(ctx, "GET", SFMT(url, "%s/test/%s", HTTP, actions[actionIndex]),
                                        NULL, 0, NULL);
                bctx->bytes = result.bytes;
                if (!processResponse(bctx, &result, url, startTime)) {
                    return;
                }
            }
            if (bctx->fatalError) break;
        }
        if (bctx->fatalError) break;

        // Cleanup connection context
        freeConnectionCtx(ctx);
        bctx->connCtx = NULL;
    }
    finishBenchContext(bctx, 4, "actions");
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
    int           warm;

    initBenchContext(bctx, "Auth", "Benchmarking digest authentication...");

    // Run both warm (reuse connection) and cold (new connection each time) tests
    for (warm = 1; warm >= 0; warm--) {
        cchar *name = warm ? "digest_with_session" : "digest_cold";

        // Initialize result for this test type
        SFMT(desc, "  Running %s tests for %.1f seconds...", warm ? "warm" : "cold", (duration / 2) / 1000.0);
        bctx->results[!warm] = initResult(name, bctx->recordResults, desc);

        // Create connection context
        ctx = createConnectionCtx(warm, URL_TIMEOUT_MS);
        bctx->connCtx = ctx;
        bctx->classIndex = !warm;

        groupStart = rGetTicks();
        while (rGetTicks() - groupStart < duration / 2) {
            // Get connection from context
            up = getConnection(ctx);
            urlSetAuth(up, "bench", "password", "digest");

            startTime = rGetTicks();
            result.status = urlFetch(up, "GET", SFMT(url, "%s/auth/secret.html", HTTP), NULL, 0, NULL);
            urlGetResponse(up);  // Consume response to enable connection reuse
            bctx->bytes = up->rxLen;
            releaseConnection(ctx);

            if (!processResponse(bctx, &result, url, startTime)) {
                return;
            }
        }
        if (bctx->fatalError) break;

        // Cleanup connection context
        freeConnectionCtx(ctx);
        bctx->connCtx = NULL;
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
    int           classIndex, warm;

    initBenchContext(bctx, "HTTPS", "Benchmarking HTTPS (URL library)...");
    setupTotalUnits(bctx, duration, true);

    // Run both warm (reuse connection) and cold (new connection each time) tests
    for (warm = 1; warm >= 0; warm--) {
        cchar *suffix = warm ? "warm" : "cold";
        int   resultOffset = warm ? 0 : 4;

        if (bctx->recordResults) {
            tinfo("  Running %s tests...", warm ? "warm" : "cold");
        }
        // Initialize results for this test type
        for (classIndex = 0; fileClasses[classIndex].name; classIndex++) {
            fc = &fileClasses[classIndex];
            SFMT(name, "%s_%s", fc->name, suffix);
            bctx->results[resultOffset + classIndex] = bctx->recordResults ? createBenchResult(name) : NULL;
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
            if (bctx->recordResults) {
                tinfo("    Testing %s for %.1f seconds...", fc->name, groupDuration / 1000.0);
            }
            groupStart = rGetTicks();
            bctx->classIndex = classIndex;
            bctx->bytes = fc->size;

            while (rGetTicks() - groupStart < groupDuration) {
                startTime = rGetTicks();
                result = executeRequest(ctx, "GET", SFMT(url, "%s/%s", HTTPS, fc->file), NULL, 0, NULL);
                if (!processResponse(bctx, &result, url, startTime)) {
                    return;
                }
            }
            if (bctx->fatalError) break;
        }
        if (bctx->fatalError) break;

        // Cleanup connection context
        freeConnectionCtx(ctx);
        bctx->connCtx = NULL;
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
        // Connection closed or error - resume fiber
        rResumeFiber(benchData->fiber, 0);
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
    char               *url, ubuf[80];
    Ticks              startTime, reqStart;

    char desc[80];

    initBenchContext(bctx, "WebSocket", "Benchmarking WebSockets...");
    SFMT(desc, "  Running echo tests for %.1f seconds...", duration / 1000.0);
    bctx->results[0] = initResult("websocket_echo", bctx->recordResults, desc);
    startTime = rGetTicks();

    // WebSockets always use cold connections (new connection per upgrade)
    ctx = createConnectionCtx(false, URL_TIMEOUT_MS);
    bctx->connCtx = ctx;
    bctx->bytes = 0;

    while (rGetTicks() - startTime < duration) {
        // Prepare benchmark data
        benchData.messagesRemaining = 10;  // Send 10 messages per connection
        benchData.result = bctx->results[0];
        benchData.startTime = 0;
        benchData.fiber = rGetFiber();

        // Create new WebSocket connection for each batch
        url = sreplace(SFMT(ubuf, "%s/test/ws/", HTTP), "http", "ws");

        reqStart = rGetTicks();
        result.status = urlWebSocket(url, (WebSocketProc) webSocketBenchCallback, &benchData, NULL);

        if (!processResponse(bctx, &result, url, reqStart)) {
            rFree(url);
            ttrue(false, "TESTME_STOP: Stopping benchmark due to WebSocket error");
            return;
        }
        rFree(url);
    }
    freeConnectionCtx(ctx);
    finishBenchContext(bctx, 1, "websockets");
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
    char          url[256], body[1024], desc[80];
    ssize         bodyLen;
    int           cycle, reqType;

    initBenchContext(bctx, "Mixed", "Benchmarking mixed workload...");
    SFMT(desc, "  Running mixed tests for %.1f seconds...", duration / 1000.0);
    bctx->results[0] = initResult("mixed_workload", bctx->recordResults, desc);
    ctx = createConnectionCtx(true, URL_TIMEOUT_MS);
    bctx->connCtx = ctx;
    up = getConnection(ctx);

    // @@ Change this to read the 1K.txt file
    // Fill upload body with test data
    for (bodyLen = 0; bodyLen < (ssize) sizeof(body) - 1; bodyLen++) {
        body[bodyLen] = 'A' + (bodyLen % 26);
    }
    body[bodyLen] = '\0';

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
            // 20% - Action handler (alternate between simple(success) and json(show))
            if (cycle % 2 == 0) {
                reqResult.status = urlFetch(up, "GET", SFMT(url, "%s/test/success", HTTP), NULL, 0, NULL);
            } else {
                reqResult.status = urlFetch(up, "GET", SFMT(url, "%s/test/show", HTTP), NULL, 0, NULL);
            }
            urlGetResponse(up);
            bctx->bytes = up->rxLen;

        } else {
            // 10% - Upload (PUT request)
            reqResult.status = urlFetch(up, "PUT", SFMT(url, "%s/upload/bench-%d.txt", HTTP, getpid()),
                                        body, (size_t) bodyLen, NULL);
            urlGetResponse(up);
            bctx->bytes = bodyLen;
        }
        if (!processResponse(bctx, &reqResult, url, startTime)) {
            return;
        }
        urlClose(up);
        cycle++;
    }
    freeConnectionCtx(ctx);
    finishBenchContext(bctx, 1, "mixed");
}

/*
   Run soak test - one complete sweep of all benchmarks
   Warms up server, caches, and allows JIT optimizations
 */
static void runSoakTest(void)
{
    Ticks totalSoakDuration, perGroupDuration;
    int   numGroups = 8;

    totalSoakDuration = getSoakDuration();
    perGroupDuration = totalSoakDuration / numGroups;

    // Note: No minimum duration enforced - soak is for warmup only
    bctx->recordResults = false;
    tinfo("Soak phase: Warming up all code paths...");
    tinfo("  Running static file requests (%.1f secs)...", perGroupDuration / 1000.0);
    benchStaticFiles(perGroupDuration);
    if (bctx->fatalError) return;

    tinfo("  Running HTTPS requests (%.1f secs)...", perGroupDuration / 1000.0);
    benchHTTPS(perGroupDuration);
    if (bctx->fatalError) return;

    tinfo("  Running WebSocket requests (%.1f secs)...", perGroupDuration / 1000.0);
    benchWebSockets(perGroupDuration);
    if (bctx->fatalError) return;

    tinfo("  Running PUT requests (%.1f secs)...", perGroupDuration / 1000.0);
    benchPut(perGroupDuration);
    if (bctx->fatalError) return;

    tinfo("  Running multipart uploads (%.1f secs)...", perGroupDuration / 1000.0);
    benchMultipartUpload(perGroupDuration);
    if (bctx->fatalError) return;

    tinfo("  Running auth requests (%.1f secs)...", perGroupDuration / 1000.0);
    benchAuth(perGroupDuration);
    if (bctx->fatalError) return;

    tinfo("  Running action requests (%.1f secs)...", perGroupDuration / 1000.0);
    benchActions(perGroupDuration);
    if (bctx->fatalError) return;

    tinfo("  Running mixed workload (%.1f secs)...", perGroupDuration / 1000.0);
    benchMixed(perGroupDuration);
    if (bctx->fatalError) return;
    tinfo("Soak phase complete - all code paths warmed");
}

/*
   Test: Benchmark static file serving
 */
static void testBenchmarkStatic(void)
{
    Ticks duration = getBenchDuration();

    bctx->recordResults = true;
    tinfo("=== Benchmarking Static File Serving (%lld secs) ===", (long long) duration / TPS);
    benchStaticFiles(duration);
}

/*
   Test: Benchmark PUT requests
 */
static void testBenchmarkPut(void)
{
    Ticks duration = getBenchDuration();

    bctx->recordResults = true;
    tinfo("=== Benchmarking PUT Requests (%lld secs) ===", (long long) duration / TPS);
    benchPut(duration);
}

/*
   Test: Benchmark multipart upload requests
 */
static void testBenchmarkMultipartUpload(void)
{
    Ticks duration = getBenchDuration();

    bctx->recordResults = true;
    tinfo("=== Benchmarking Multipart Uploads (%lld secs) ===", (long long) duration / TPS);
    benchMultipartUpload(duration);
}

/*
   Test: Benchmark authenticated routes
 */
static void testBenchmarkAuth(void)
{
    Ticks duration = getBenchDuration();

    bctx->recordResults = true;
    tinfo("=== Benchmarking Authentication (%lld secs) ===", (long long) duration / TPS);
    benchAuth(duration);
}

/*
   Test: Benchmark HTTPS performance
 */
static void testBenchmarkHTTPS(void)
{
    Ticks duration = getBenchDuration();

    bctx->recordResults = true;
    tinfo("=== Benchmarking HTTPS (%lld secs) ===", (long long) duration / TPS);
    benchHTTPS(duration);
}

/*
   Test: Benchmark action handlers
 */
static void testBenchmarkActions(void)
{
    Ticks duration = getBenchDuration();

    bctx->recordResults = true;
    tinfo("=== Benchmarking Action Handlers (%lld secs) ===", (long long) duration / TPS);
    benchActions(duration);
}

/*
   Test: Benchmark mixed workload
 */
static void testBenchmarkMixed(void)
{
    Ticks duration = getBenchDuration();

    bctx->recordResults = true;
    tinfo("=== Benchmarking Mixed Workload (%lld secs) ===", (long long) duration / TPS);
    benchMixed(duration);
}

/*
   Test: Benchmark WebSocket operations
 */
static void testBenchmarkWebSockets(void)
{
    Ticks duration = getBenchDuration();

    bctx->recordResults = true;
    tinfo("=== Benchmarking WebSockets (%lld secs) ===", (long long) duration / TPS);
    benchWebSockets(duration);
}

/*
   Test: Benchmark static file serving with raw sockets (HTTP)
 */
static void testBenchmarkStaticRawHTTP(void)
{
    Ticks duration = getBenchDuration();
    char  *host, *portStr;
    int   port;

    // Parse HTTP endpoint for host and port
    if (!HTTP || !scontains(HTTP, "://")) {
        tinfo("Skipping raw HTTP benchmark - invalid endpoint");
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

    bctx->recordResults = true;
    tinfo("=== Benchmarking Static Files (Raw HTTP) (%lld secs) ===", (long long) duration / TPS);
    benchStaticFilesRaw(duration, host, port, false);
    rFree(host);
}

/*
   Test: Benchmark static file serving with raw sockets (HTTPS)
 */
static void testBenchmarkStaticRawHTTPS(void)
{
    Ticks duration = getBenchDuration();
    char  *host, *portStr;
    int   port;

    // Parse HTTPS endpoint for host and port
    if (!HTTPS || !scontains(HTTPS, "://")) {
        tinfo("Skipping raw HTTPS benchmark - invalid endpoint");
        return;
    }
    host = sclone(HTTPS + 8);  // Skip "https://"
    portStr = schr(host, ':');
    if (portStr) {
        *portStr = '\0';
        port = atoi(portStr + 1);
    } else {
        port = 443;
    }

    bctx->recordResults = true;
    tinfo("=== Benchmarking Static Files (Raw HTTPS) (%lld secs) ===", (long long) duration / TPS);
    benchStaticFilesRaw(duration, host, port, true);
    rFree(host);
}

/*
   Test: Benchmark using wrk tool for maximum raw throughput
   Invokes external wrk benchmarking tool if available
 */
static void testBenchmarkWrk(void)
{
    char         *host, *portStr, cmd[1024], tmpfile[256], *output, *line;
    int          port, rc;
    BenchResult  *result;
    double       reqPerSec, avgLatency;

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

    // Check if wrk is available
    rc = system("command -v wrk >/dev/null 2>&1");
    if (rc != 0) {
        tinfo("SKIP: wrk not installed - install from https://github.com/wg/wrk");
        rFree(host);
        return;
    }

    tinfo("Target: http://%s:%d/static/1K.txt", host, port);
    tinfo("Threads: 12, Connections: 40, Duration: 30s");
    printf("\n");

    // Run wrk benchmark and capture output to temp file
    SFMT(tmpfile, "/tmp/wrk-bench-%d.txt", getpid());
    SFMT(cmd, "wrk -t12 -c40 -d30s http://%s:%d/static/1K.txt 2>&1 | tee %s",
         host, port, tmpfile);
    rc = system(cmd);

    if (rc != 0) {
        tinfo("Warning: wrk command failed with exit code %d", rc);
        rFree(host);
        return;
    }

    printf("\n");

    // Read and parse wrk output
    output = rReadFile(tmpfile, NULL);
    if (!output) {
        tinfo("Warning: Could not read wrk output");
        rFree(host);
        return;
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
    result = createBenchResult("max_raw_throughput");
    result->requestsPerSec = reqPerSec;
    result->avgTime = avgLatency;
    result->iterations = (int)(reqPerSec * 30);  // Approximate: req/sec * 30 seconds
    result->totalTime = 30000;  // 30 seconds in milliseconds
    result->minTime = 0;
    result->maxTime = 0;
    result->p95Time = 0;
    result->p99Time = 0;
    result->bytesTransferred = result->iterations * 1024;  // Approximate: 1KB per request
    result->errors = 0;

    // Print and save results
    printBenchResult(result);
    saveBenchGroup("max_throughput", &result, 1);
    freeBenchResult(result);

    // Cleanup
    unlink(tmpfile);
    rFree(output);
    rFree(host);
}

/*
   Fiber main function - runs all benchmarks
 */
static void fiberMain(void *data)
{
    cchar *testClass;
    Ticks duration;
    char  *host, *portStr;
    int   port;

    // Setup endpoints from web.json5
    if (!benchSetup(&HTTP, &HTTPS)) {
        rStop();
        return;
    }

    // Check for TESTME_STOP flag
    if (getenv("TESTME_STOP")) {
        bctx->stopOnErrors = true;
        tinfo("TESTME_STOP: Will stop immediately on any request error");
    }

    // Check for TESTME_CLASS to run a single benchmark class
    testClass = getenv("TESTME_CLASS");
    if (testClass && *testClass) {
        // Validate test class
        if (!smatch(testClass, "static") && !smatch(testClass, "https") &&
            !smatch(testClass, "raw_http") && !smatch(testClass, "raw_https") &&
            !smatch(testClass, "put") && !smatch(testClass, "multipart") &&
            !smatch(testClass, "auth") && !smatch(testClass, "actions") &&
            !smatch(testClass, "mixed") && !smatch(testClass, "websockets") &&
            !smatch(testClass, "max_throughput")) {
            tinfo("Error: Invalid TESTME_CLASS='%s'", testClass);
            tinfo("Valid values: static, https, raw_http, raw_https, put, multipart, auth, actions, mixed, websockets, max_throughput");
            rFree(HTTP);
            rFree(HTTPS);
            rStop();
            return;
        }
        // Configure duration for single test group
        configureDuration(1);
        printf("\n");
        printf("=========================================\n");
        printf("Web Server Performance Benchmark Suite\n");
        printf("Single Class: %s\n", testClass);
        printf("=========================================\n");
        printf("HTTP:  %s\n", HTTP);
        printf("HTTPS: %s\n", HTTPS);
        printf("=========================================\n");
        printf("\n");

        // Run targeted soak for this class
        duration = getSoakDuration();
        bctx->recordResults = false;
        tinfo("=== Phase 1: Soak - %s (%.1f secs) ===", testClass, duration / 1000.0);
        if (smatch(testClass, "static")) {
            benchStaticFiles(duration);
        } else if (smatch(testClass, "https")) {
            benchHTTPS(duration);
        } else if (smatch(testClass, "raw_http")) {
            host = sclone(HTTP + 7);
            portStr = schr(host, ':');
            if (portStr) {
                *portStr = '\0';
                port = atoi(portStr + 1);
            } else {
                port = 80;
            }
            benchStaticFilesRaw(duration, host, port, false);
            rFree(host);
        } else if (smatch(testClass, "raw_https")) {
            host = sclone(HTTPS + 8);
            portStr = schr(host, ':');
            if (portStr) {
                *portStr = '\0';
                port = atoi(portStr + 1);
            } else {
                port = 443;
            }
            benchStaticFilesRaw(duration, host, port, true);
            rFree(host);
        } else if (smatch(testClass, "put")) {
            benchPut(duration);
        } else if (smatch(testClass, "multipart")) {
            benchMultipartUpload(duration);
        } else if (smatch(testClass, "auth")) {
            benchAuth(duration);
        } else if (smatch(testClass, "actions")) {
            benchActions(duration);
        } else if (smatch(testClass, "mixed")) {
            benchMixed(duration);
        } else if (smatch(testClass, "websockets")) {
            benchWebSockets(duration);
        } else if (smatch(testClass, "max_throughput")) {
            // No soak phase for max_throughput - it's an independent benchmark tool
        }
        if (bctx->fatalError) goto done;
        recordInitialMemory();
        printf("\n");

        // Run the benchmark
        tinfo("=== Phase 2: Benchmark - %s ===", testClass);
        if (smatch(testClass, "static")) {
            testBenchmarkStatic();
        } else if (smatch(testClass, "https")) {
            testBenchmarkHTTPS();
        } else if (smatch(testClass, "raw_http")) {
            testBenchmarkStaticRawHTTP();
        } else if (smatch(testClass, "raw_https")) {
            testBenchmarkStaticRawHTTPS();
        } else if (smatch(testClass, "put")) {
            testBenchmarkPut();
        } else if (smatch(testClass, "multipart")) {
            testBenchmarkMultipartUpload();
        } else if (smatch(testClass, "auth")) {
            testBenchmarkAuth();
        } else if (smatch(testClass, "actions")) {
            testBenchmarkActions();
        } else if (smatch(testClass, "mixed")) {
            testBenchmarkMixed();
        } else if (smatch(testClass, "websockets")) {
            testBenchmarkWebSockets();
        } else if (smatch(testClass, "max_throughput")) {
            testBenchmarkWrk();
        }
        if (bctx->fatalError) goto done;

        // Save results
        tinfo("=== Phase 3: Analysis ===");
        recordFinalMemory();
        saveFinalResults();

    } else {
        // Run all benchmarks
        // Configure duration from TESTME_DURATION
        // Active test groups: static, HTTPS, raw HTTP, raw HTTPS, uploads, multipart, auth, actions, mixed, websockets, wrk (11 groups)
        configureDuration(11);

        printf("\n");
        printf("=========================================\n");
        printf("Web Server Performance Benchmark Suite\n");
        printf("=========================================\n");
        printf("HTTP:  %s\n", HTTP);
        printf("HTTPS: %s\n", HTTPS);
        printf("=========================================\n");
        printf("\n");

        // Phase 1: Soak (warm up)
        tinfo("=== Phase 1: Soak ===");
        runSoakTest();
        if (bctx->fatalError) goto done;
        recordInitialMemory();
        printf("\n");

        // Phase 2: Benchmarks (measurement)
        tinfo("=== Phase 2: Benchmarks ===");
        testBenchmarkWrk();
        if (bctx->fatalError) goto done;
        testBenchmarkStatic();
        if (bctx->fatalError) goto done;
        testBenchmarkHTTPS();
        if (bctx->fatalError) goto done;
        testBenchmarkStaticRawHTTP();
        if (bctx->fatalError) goto done;
        testBenchmarkStaticRawHTTPS();
        if (bctx->fatalError) goto done;
        testBenchmarkWebSockets();
        if (bctx->fatalError) goto done;
        testBenchmarkPut();
        if (bctx->fatalError) goto done;
        testBenchmarkMultipartUpload();
        if (bctx->fatalError) goto done;
        testBenchmarkAuth();
        if (bctx->fatalError) goto done;
        testBenchmarkActions();
        if (bctx->fatalError) goto done;
        testBenchmarkMixed();
        if (bctx->fatalError) goto done;

        // Phase 3: Save results
        tinfo("=== Phase 3: Analysis ===");
        recordFinalMemory();
        saveFinalResults();
    }

done:

    // Check for errors and emit final pass/fail status
    printf("\n");
    printf("=========================================\n");
    if (bctx->totalErrors > 0) {
        printf("BENCHMARK RESULT: FAILED (%d errors)\n", bctx->totalErrors);
        printf("=========================================\n");
        ttrue(false, "Benchmark suite completed with %d total errors", bctx->totalErrors);
    } else {
        printf("BENCHMARK RESULT: PASSED (no errors)\n");
        printf("=========================================\n");
        ttrue(true, "Benchmark suite completed successfully with no errors");
    }

    rFree(HTTP);
    rFree(HTTPS);
    rStop();
}

/*
   Main entry point
 */
int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
    return (bctx->fatalError || bctx->totalErrors > 0) ? 1 : 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
