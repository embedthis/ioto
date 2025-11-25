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
    double multiplier;                                // Fraction of base iterations (1.0 = full, 0.25 = 25%)
} FileClass;

static FileClass fileClasses[] = {
    { "1KB",   "static/1K.txt",   1024,       1.0  }, // Full iterations for small files
    { "10KB",  "static/10K.txt",  10240,      1.0  }, // Full iterations
    { "100KB", "static/100K.txt", 102400,     0.25 }, // 25% iterations for large files
    { "1MB",   "static/1M.txt",   1048576,    0.25 }, // 25% iterations for very large files
    { NULL,    NULL,              0,          0    }
};

// Code

/*
   Calculate group duration with multiplier and enforce minimum
 */
static Ticks calculateGroupDuration(Ticks duration, double multiplier)
{
    Ticks groupDuration;

    groupDuration = (Ticks) (duration * multiplier);
    if (groupDuration < MIN_GROUP_DURATION_MS) {
        groupDuration = MIN_GROUP_DURATION_MS;
    }
    return groupDuration;
}

/*
   Initialize a single benchmark result
 */
static BenchResult *initResult(cchar *name, bool recordResults)
{
    return recordResults ? createBenchResult(name) : NULL;
}

/*
   Initialize results array from file classes
 */
static void initFileClassResults(BenchResult **results, FileClass *classes, int count, bool recordResults)
{
    int i;

    for (i = 0; i < count && classes[i].name; i++) {
        results[i] = initResult(classes[i].name, recordResults);
    }
}

/*
   Record timing result for a single request
   Pass isSuccess=true if the request succeeded, false otherwise
 */
static void recordRequest(BenchResult *result, bool isSuccess, Ticks elapsed, ssize bytes)
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
    }
}

/*
   Finalize benchmark results: calculate stats, print, save, and cleanup
 */
static void finalizeResults(BenchResult **results, int count, cchar *groupName)
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
   Extract Content-Length from raw HTTP headers
   Returns -1 if not found or invalid
 */
static ssize parseContentLength(cchar *headers, size_t len)
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
static ssize readHeaders(RSocket *sp, char *buf, size_t bufsize, Ticks deadline, ssize *bodyStart, ssize *bodyLen)
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
   Helper: Execute a single raw socket request
   Returns true on success, false on error
 */
static bool executeRawRequest(RSocket **spPtr, cchar *request, cchar *host, int port, bool useTls, bool reuseSocket,
                              Ticks deadline, ssize expectedSize)
{
    RSocket *sp;
    char    headers[8192], *body;
    ssize   bodyStart, bodyInHeaders, headerLen, contentLen, bodyRead, nbytes;

    // Allocate socket if needed (cold connections)
    if (!reuseSocket || !*spPtr) {
        if (*spPtr && !reuseSocket) {
            rFreeSocket(*spPtr);
        }
        *spPtr = sp = rAllocSocket();
        if (useTls) {
            rSetTls(sp);
        }
        rSetSocketLinger(sp, 0);
    } else {
        sp = *spPtr;
    }
    // Connect if needed
    if (sp->fd == INVALID_SOCKET) {
        if (rConnectSocket(sp, host, port, deadline) < 0) {
            return false;
        }
    }
    // Send request
    if (rWriteSocket(sp, request, slen(request), deadline) < 0) {
        if (reuseSocket) {
            rCloseSocket(sp);
            rResetSocket(sp);
        }
        return false;
    }
    // Read headers (may also read some body data)
    headerLen = readHeaders(sp, headers, sizeof(headers), deadline, &bodyStart, &bodyInHeaders);
    if (headerLen < 0) {
        if (reuseSocket) {
            rCloseSocket(sp);
            rResetSocket(sp);
        }
        return false;
    }
    // Parse Content-Length
    contentLen = parseContentLength(headers, (size_t) bodyStart);
    if (contentLen < 0 || contentLen > expectedSize * 2) {
        if (reuseSocket) {
            rCloseSocket(sp);
            rResetSocket(sp);
        }
        return false;
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
            rFree(body);
            if (reuseSocket) {
                rCloseSocket(sp);
                rResetSocket(sp);
            }
            return false;
        }
        bodyRead += nbytes;
    }
    rFree(body);
    return true;
}

/*
   Benchmark static file serving using raw sockets (no URL library overhead)
   Tests: 1KB, 10KB, 100KB, 1MB files using duration-based testing
   This provides the fastest possible client for accurate server benchmarking
 */
static void benchStaticFilesRaw(Ticks duration, bool recordResults, cchar *host, int port, bool useTls)
{
    FileClass   *fc;
    BenchResult *results[8];    // 4 file classes x 2 (warm + cold)
    RSocket     *sp;
    Ticks       startTime, elapsed, groupStart, groupDuration, deadline;
    char        name[64], request[512];
    bool        success, reuseSocket;
    int         classIndex, warmCold;
    double      totalUnits;

    sp = NULL;

    // Calculate total weighted units (considering multipliers and warm/cold)
    totalUnits = 0;
    for (int i = 0; fileClasses[i].name; i++) {
        totalUnits += fileClasses[i].multiplier;
    }
    totalUnits *= 2;                   // Account for warm and cold

    // Run both warm (reuse socket) and cold (new socket each time) tests
    for (warmCold = 0; warmCold < 2; warmCold++) {
        reuseSocket = (warmCold == 0); // 0=warm, 1=cold
        cchar *suffix = reuseSocket ? "raw_warm" : "raw_cold";
        cchar *connection = reuseSocket ? "keep-alive" : "close";
        int   resultOffset = reuseSocket ? 0 : 4;

        // Initialize results for this test type
        for (classIndex = 0; fileClasses[classIndex].name; classIndex++) {
            fc = &fileClasses[classIndex];
            snprintf(name, sizeof(name), "%s_%s", fc->name, suffix);
            results[resultOffset + classIndex] = recordResults ? createBenchResult(name) : NULL;
        }
        // Allocate socket for warm tests (reused across all requests)
        if (reuseSocket) {
            sp = rAllocSocket();
            if (useTls) {
                rSetTls(sp);
            }
            rSetSocketLinger(sp, 0);
        }
        // Run tests for each file class
        for (classIndex = 0; fileClasses[classIndex].name; classIndex++) {
            fc = &fileClasses[classIndex];
            // Allocate time proportionally based on multiplier
            groupDuration = (Ticks) (duration * fc->multiplier / totalUnits);
            if (groupDuration < MIN_GROUP_DURATION_MS) {
                groupDuration = MIN_GROUP_DURATION_MS;
            }
            groupStart = rGetTicks();

            // Pre-format HTTP request
            snprintf(request, sizeof(request),
                     "GET /%s HTTP/1.1\r\n"
                     "Host: %s\r\n"
                     "Connection: %s\r\n"
                     "\r\n",
                     fc->file, host, connection);

            while (rGetTicks() - groupStart < groupDuration) {
                startTime = rGetTicks();
                deadline = startTime + URL_TIMEOUT_MS;

                success = executeRawRequest(&sp, request, host, port, useTls, reuseSocket, deadline, fc->size);

                if (recordResults) {
                    elapsed = rGetTicks() - startTime;
                    recordRequest(results[resultOffset + classIndex], success, elapsed, fc->size);
                }
            }
        }
        // Cleanup socket
        if (sp) {
            rFreeSocket(sp);
            sp = NULL;
        }
    }
    if (recordResults) {
        finalizeResults(results, 8, useTls ? "static_files_raw_https" : "static_files_raw_http");
    }
}

/*
   Benchmark static file serving with keep-alive vs cold connections
   Tests: 1KB, 10KB, 100KB, 1MB files using duration-based testing
 */
static void benchStaticFiles(Ticks duration, bool recordResults)
{
    FileClass   *fc;
    BenchResult *results[8];  // 4 file classes x 2 (warm + cold)
    Url         *up;
    Ticks       startTime, elapsed, groupStart, groupDuration;
    char        url[256], name[64];
    bool        reuseConnection;
    int         classIndex, warmCold, status;
    double      totalUnits;

    up = NULL;

    // Calculate total weighted units (considering multipliers and warm/cold)
    totalUnits = 0;
    for (int i = 0; fileClasses[i].name; i++) {
        totalUnits += fileClasses[i].multiplier;
    }
    totalUnits *= 2;                       // Account for warm and cold

    // Run both warm (reuse connection) and cold (new connection each time) tests
    for (warmCold = 0; warmCold < 2; warmCold++) {
        reuseConnection = (warmCold == 0); // 0=warm, 1=cold
        cchar *suffix = reuseConnection ? "warm" : "cold";
        int   resultOffset = reuseConnection ? 0 : 4;

        // Initialize results for this test type
        for (classIndex = 0; fileClasses[classIndex].name; classIndex++) {
            fc = &fileClasses[classIndex];
            snprintf(name, sizeof(name), "%s_%s", fc->name, suffix);
            results[resultOffset + classIndex] = recordResults ? createBenchResult(name) : NULL;
        }

        // Allocate URL for warm tests (reused across all requests)
        if (reuseConnection) {
            up = urlAlloc(URL_NO_LINGER);
            urlSetTimeout(up, URL_TIMEOUT_MS);
        }

        // Run tests for each file class
        for (classIndex = 0; fileClasses[classIndex].name; classIndex++) {
            fc = &fileClasses[classIndex];
            // Allocate time proportionally based on multiplier
            groupDuration = (Ticks) (duration * fc->multiplier / totalUnits);
            if (groupDuration < MIN_GROUP_DURATION_MS) {
                groupDuration = MIN_GROUP_DURATION_MS;
            }
            groupStart = rGetTicks();

            while (rGetTicks() - groupStart < groupDuration) {
                // Allocate URL for cold tests
                if (!reuseConnection) {
                    up = urlAlloc(URL_NO_LINGER);
                    urlSetTimeout(up, URL_TIMEOUT_MS);
                }

                startTime = rGetTicks();
                status = urlFetch(up, "GET", SFMT(url, "%s/%s", HTTP, fc->file), NULL, 0, NULL);
                urlGetResponse(up);  // Consume response to enable connection reuse

                if (recordResults) {
                    elapsed = rGetTicks() - startTime;
                    recordRequest(results[resultOffset + classIndex], status == 200, elapsed, fc->size);
                }

                // Free URL for cold tests
                if (!reuseConnection) {
                    urlClose(up);
                    urlFree(up);
                }
            }
        }
        // Cleanup URL for warm tests
        if (reuseConnection && up) {
            urlClose(up);
            urlFree(up);
            up = NULL;
        }
    }
    if (recordResults) {
        finalizeResults(results, 8, "static_files");
    }
}

/*
   Benchmark file uploads with keep-alive vs cold connections
   Tests: 1KB, 10KB, 100KB file uploads using PUT with duration-based testing
 */
static void benchUploads(Ticks duration, bool recordResults)
{
    BenchResult *results[6];  // 3 file classes x 2 (warm + cold)
    Url         *up;
    Ticks       startTime, elapsed, groupStart, groupDuration;
    FileClass   *fc;
    ssize       fileSize;
    char        path[256], url[256], name[64], *fileData;
    bool        reuseConnection;
    int         classIndex, warmCold, status, counter;
    double      totalUnits;

    // Use first 3 file classes for uploads (1KB, 10KB, 100KB)
    // Skip 1MB to keep upload tests reasonable

    up = NULL;

    // Calculate total weighted units (3 file classes, warm + cold)
    totalUnits = 0;
    for (int i = 0; i < 3 && fileClasses[i].name; i++) {
        totalUnits += fileClasses[i].multiplier;
    }
    totalUnits *= 2;                       // Account for warm and cold

    // Run both warm (reuse connection) and cold (new connection each time) tests
    for (warmCold = 0; warmCold < 2; warmCold++) {
        reuseConnection = (warmCold == 0); // 0=warm, 1=cold
        cchar *suffix = reuseConnection ? "warm" : "cold";
        int   resultOffset = reuseConnection ? 0 : 3;

        // Initialize results for this test type
        for (classIndex = 0; classIndex < 3 && fileClasses[classIndex].name; classIndex++) {
            fc = &fileClasses[classIndex];
            snprintf(name, sizeof(name), "%s_%s", fc->name, suffix);
            results[resultOffset + classIndex] = recordResults ? createBenchResult(name) : NULL;
        }
        // Allocate URL for warm tests (reused across all requests)
        if (reuseConnection) {
            up = urlAlloc(URL_NO_LINGER);
            urlSetTimeout(up, URL_TIMEOUT_MS);
        }
        // Run tests for each file class
        for (classIndex = 0; classIndex < 3 && fileClasses[classIndex].name; classIndex++) {
            fc = &fileClasses[classIndex];
            // Allocate time proportionally based on multiplier
            groupDuration = (Ticks) (duration * fc->multiplier / totalUnits);
            if (groupDuration < MIN_GROUP_DURATION_MS) {
                groupDuration = MIN_GROUP_DURATION_MS;
            }
            fileData = rReadFile(SFMT(path, "site/%s", fc->file), (size_t*) &fileSize);
            if (!fileData) {
                continue;
            }
            groupStart = rGetTicks();
            counter = 0;

            while (rGetTicks() - groupStart < groupDuration) {
                // Allocate URL for cold tests
                if (!reuseConnection) {
                    up = urlAlloc(URL_NO_LINGER);
                    urlSetTimeout(up, URL_TIMEOUT_MS);
                }

                startTime = rGetTicks();
                snprintf(url, sizeof(url), "%s/upload/bench-%d-%d.txt", HTTP, getpid(), counter);
                status = urlFetch(up, "PUT", url, fileData, (size_t) fileSize, NULL);
                urlGetResponse(up);  // Consume response to enable connection reuse
                elapsed = rGetTicks() - startTime;

                if (recordResults) {
                    recordRequest(results[resultOffset + classIndex], status == 201 || status == 204, elapsed,
                                  fileSize);
                }

                // Delete the uploaded file to prevent buildup
                urlFetch(up, "DELETE", url, NULL, 0, NULL);
                urlGetResponse(up);

                counter++;

                // Free URL for cold tests
                if (!reuseConnection) {
                    urlClose(up);
                    urlFree(up);
                }
            }
            rFree(fileData);
        }
        // Cleanup URL for warm tests
        if (reuseConnection && up) {
            urlClose(up);
            urlFree(up);
            up = NULL;
        }
    }
    if (recordResults) {
        finalizeResults(results, 6, "uploads");
    }
}

/*
   Benchmark action handlers
   Tests: Simple action, JSON action with warm/cold connections using duration-based testing
 */
static void benchActions(Ticks duration, bool recordResults)
{
    BenchResult *results[4];  // 2 actions x 2 (warm + cold)
    Url         *up;
    RBuf        *responseBuf;
    Ticks       startTime, elapsed, groupStart, groupDuration;
    char        url[256], name[64];
    bool        reuseConnection;
    int         warmCold, actionIndex, status, resultOffset;
    double      totalUnits;
    cchar       *suffix;
    ssize       bytes;

    // Used in URLs
    cchar *actions[] = { "success", "show", NULL };

    // Used in results
    cchar *actionNames[] = { "simple", "json" };

    up = NULL;

    // Calculate total weighted units (2 actions x 2 conditions = 4 equal tests)
    totalUnits = 4.0;

    // Run both warm (reuse connection) and cold (new connection each time) tests
    for (warmCold = 0; warmCold < 2; warmCold++) {
        reuseConnection = (warmCold == 0);  // 0=warm, 1=cold
        suffix = reuseConnection ? "warm" : "cold";
        resultOffset = reuseConnection ? 0 : 2;

        // Initialize results for both actions
        for (actionIndex = 0; actions[actionIndex]; actionIndex++) {
            snprintf(name, sizeof(name), "%s_%s", actionNames[actionIndex], suffix);
            results[resultOffset + actionIndex] = initResult(name, recordResults);
        }

        // Allocate URL for warm tests (reused across all requests)
        if (reuseConnection) {
            up = urlAlloc(URL_NO_LINGER);
            urlSetTimeout(up, URL_TIMEOUT_MS);
        }

        // Test each action
        for (actionIndex = 0; actions[actionIndex]; actionIndex++) {
            // Allocate time equally across all test cases
            groupDuration = (Ticks) (duration / totalUnits);
            if (groupDuration < MIN_GROUP_DURATION_MS) {
                groupDuration = MIN_GROUP_DURATION_MS;
            }
            groupStart = rGetTicks();
            while (rGetTicks() - groupStart < groupDuration) {
                // Allocate URL for cold tests
                if (!reuseConnection) {
                    up = urlAlloc(URL_NO_LINGER);
                    urlSetTimeout(up, URL_TIMEOUT_MS);
                }

                startTime = rGetTicks();
                status = urlFetch(up, "GET", SFMT(url, "%s/test/%s", HTTP, actions[actionIndex]), NULL, 0, NULL);
                responseBuf = urlGetResponseBuf(up);

                if (recordResults) {
                    elapsed = rGetTicks() - startTime;
                    bytes = responseBuf ? (ssize) rGetBufLength(responseBuf) : 0;
                    recordRequest(results[resultOffset + actionIndex], status == 200, elapsed, bytes);
                }

                // Free URL for cold tests
                if (!reuseConnection) {
                    urlClose(up);
                    urlFree(up);
                }
            }
        }
        // Cleanup URL for warm tests
        if (reuseConnection && up) {
            urlClose(up);
            urlFree(up);
            up = NULL;
        }
    }
    if (recordResults) {
        finalizeResults(results, 4, "actions");
    }
}

/*
   Benchmark authenticated routes with digest authentication
   Tests: Digest auth with session reuse, cold auth using duration-based testing
 */
static void benchAuth(Ticks duration, bool recordResults)
{
    BenchResult *results[2];
    Url         *up;
    Ticks       startTime, elapsed, groupStart;
    char        url[256];
    bool        reuseConnection;
    int         warmCold, status;

    up = NULL;

    // Run both warm (reuse connection) and cold (new connection each time) tests
    for (warmCold = 0; warmCold < 2; warmCold++) {
        reuseConnection = (warmCold == 0);  // 0=warm, 1=cold
        cchar *name = reuseConnection ? "digest_with_session" : "digest_cold";

        // Initialize result for this test type
        results[warmCold] = initResult(name, recordResults);

        // Allocate URL for warm tests (reused across all requests)
        if (reuseConnection) {
            up = urlAlloc(URL_NO_LINGER);
        }
        groupStart = rGetTicks();
        while (rGetTicks() - groupStart < duration / 2) {
            // Allocate URL for cold tests
            if (!reuseConnection) {
                up = urlAlloc(URL_NO_LINGER);
            }
            urlSetTimeout(up, URL_TIMEOUT_MS);
            urlSetAuth(up, "bench", "password", "digest");
            startTime = rGetTicks();
            status = urlFetch(up, "GET", SFMT(url, "%s/auth/secret.html", HTTP), NULL, 0, NULL);
            urlGetResponse(up);  // Consume response to enable connection reuse
            elapsed = rGetTicks() - startTime;

            if (recordResults) {
                recordRequest(results[warmCold], status == 200, elapsed, up->rxLen);
            }
            // Free URL for cold tests
            if (!reuseConnection) {
                urlClose(up);
                urlFree(up);
            }
        }
        // Cleanup URL for warm tests
        if (reuseConnection && up) {
            urlClose(up);
            urlFree(up);
            up = NULL;
        }
    }
    if (recordResults) {
        finalizeResults(results, 2, "auth");
    }
}

/*
   Benchmark HTTPS performance
   Tests: TLS handshakes, session reuse using duration-based testing
 */
static void benchHTTPS(Ticks duration, bool recordResults)
{
    BenchResult *results[2];
    Url         *up;
    Ticks       startTime, elapsed, groupStart;
    char        url[256];
    bool        reuseConnection;
    int         warmCold, status;

    up = NULL;

    // Run both warm (reuse connection) and cold (new connection each time) tests
    for (warmCold = 0; warmCold < 2; warmCold++) {
        reuseConnection = (warmCold == 0);  // 0=warm, 1=cold
        cchar *name = reuseConnection ? "https_session_reuse" : "https_cold_handshake";

        // Initialize result for this test type
        results[warmCold] = initResult(name, recordResults);

        // Allocate URL for warm tests (reused across all requests)
        if (reuseConnection) {
            up = urlAlloc(URL_NO_LINGER);
            urlSetTimeout(up, URL_TIMEOUT_MS);
        }
        groupStart = rGetTicks();
        while (rGetTicks() - groupStart < duration / 2) {
            // Allocate URL for cold tests
            if (!reuseConnection) {
                up = urlAlloc(URL_NO_LINGER);
                urlSetTimeout(up, URL_TIMEOUT_MS);
            }
            startTime = rGetTicks();
            status = urlFetch(up, "GET", SFMT(url, "%s/static/1K.txt", HTTPS), NULL, 0, NULL);
            urlGetResponse(up);  // Consume response to enable connection reuse

            if (recordResults) {
                elapsed = rGetTicks() - startTime;
                recordRequest(results[warmCold], status == 200, elapsed, up->rxLen);
            }
            // Free URL for cold tests
            if (!reuseConnection) {
                urlClose(up);
                urlFree(up);
            }
        }
        // Cleanup URL for warm tests
        if (reuseConnection && up) {
            urlClose(up);
            urlFree(up);
            up = NULL;
        }
    }
    if (recordResults) {
        finalizeResults(results, 2, "https");
    }
}

/*
   WebSocket benchmark data
 */
typedef struct {
    BenchResult *result;
    Ticks startTime;
    int messagesRemaining;
    bool recordResults;
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
        if (benchData->recordResults) {
            recordRequest(benchData->result, true, elapsed, (ssize) len);
        }
        // Send next message if we have more to send
        if (benchData->messagesRemaining > 0) {
            benchData->startTime = rGetTicks();
            webSocketSend(ws, "Benchmark message %d", benchData->messagesRemaining);
            benchData->messagesRemaining--;
        } else {
            // Done - close connection
            webSocketSendClose(ws, WS_STATUS_OK, "Benchmark complete");
            rResumeFiber(benchData->fiber, 0);
        }
    }
}

/*
   Benchmark WebSocket operations
   Tests: Message roundtrip time with echo server
 */
static void benchWebSockets(Ticks duration, bool recordResults)
{
    BenchResult        *result;
    WebSocketBenchData benchData;
    Url                *up;
    char               *url, ubuf[80];
    int                status;
    Ticks              startTime;

    result = initResult("websocket_echo", recordResults);
    startTime = rGetTicks();

    while (rGetTicks() - startTime < duration) {
        // Prepare benchmark data
        benchData.messagesRemaining = 10;  // Send 10 messages per connection
        benchData.recordResults = recordResults;
        benchData.result = result;
        benchData.startTime = 0;
        benchData.fiber = rGetFiber();

        // Create new WebSocket connection for each batch
        up = urlAlloc(URL_NO_LINGER /* | URL_SHOW_REQ_HEADERS | URL_SHOW_RESP_HEADERS */);
        urlSetTimeout(up, URL_TIMEOUT_MS);
        url = sreplace(SFMT(ubuf, "%s/test/ws", HTTP), "http", "ws");

        status = urlFetch(up, "GET", url, NULL, 0, NULL);
        if (status == 101) {
            // Upgrade successful - activate WebSocket with callback
            urlWebSocketAsync(up, (WebSocketProc) webSocketBenchCallback, &benchData);
            // Wait till connection is closed or fiber resumed
            urlWait(up);

        } else if (recordResults && result) {
            result->errors++;
        }
        rFree(url);
        urlClose(up);
        urlFree(up);
    }
    if (recordResults && result) {
        calculateStats(result);
        printBenchResult(result);
        saveBenchGroup("websockets", &result, 1);
        freeBenchResult(result);
    }
}

/*
   Benchmark mixed workload - realistic traffic pattern
   70% GET requests, 20% actions, 10% uploads
 */
static void benchMixed(Ticks duration, bool recordResults)
{
    BenchResult *result;
    Url         *up;
    Ticks       startTime, elapsed, groupStart;
    char        url[256];
    char        body[1024];
    int         status, cycle, reqType;
    ssize       bodyLen;

    result = initResult("mixed_workload", recordResults);
    up = urlAlloc(0);
    urlSetTimeout(up, URL_TIMEOUT_MS);

    // Fill upload body with test data
    for (bodyLen = 0; bodyLen < (ssize) sizeof(body) - 1; bodyLen++) {
        body[bodyLen] = 'A' + (bodyLen % 26);
    }
    body[bodyLen] = '\0';

    groupStart = rGetTicks();
    cycle = 0;

    while (rGetTicks() - groupStart < duration) {
        // Determine request type based on cycle (70% GET, 20% action, 10% upload)
        // Pattern: G G G G G G G A A U (10 requests = 7 GET + 2 action + 1 upload)
        reqType = cycle % 10;

        startTime = rGetTicks();

        if (reqType < 7) {
            // 70% - GET static file (alternate between file sizes)
            if (cycle % 4 == 0) {
                status = urlFetch(up, "GET", SFMT(url, "%s/static/1K.txt", HTTP), NULL, 0, NULL);
            } else if (cycle % 4 == 1) {
                status = urlFetch(up, "GET", SFMT(url, "%s/static/10K.txt", HTTP), NULL, 0, NULL);
            } else {
                status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
            }
            urlGetResponse(up);
            elapsed = rGetTicks() - startTime;
            if (recordResults) {
                recordRequest(result, status == 200, elapsed, up->rxLen);
            }

        } else if (reqType < 9) {
            // 20% - Action handler (alternate between simple(success) and json(show))
            if (cycle % 2 == 0) {
                status = urlFetch(up, "GET", SFMT(url, "%s/test/success", HTTP), NULL, 0, NULL);
            } else {
                status = urlFetch(up, "GET", SFMT(url, "%s/test/show", HTTP), NULL, 0, NULL);
            }
            urlGetResponse(up);
            elapsed = rGetTicks() - startTime;
            if (recordResults) {
                recordRequest(result, status == 200, elapsed, up->rxLen);
            }

        } else {
            // 10% - Upload (PUT request)
            status = urlFetch(up, "PUT", SFMT(url, "%s/upload/bench-%d.txt", HTTP, getpid()), body, (size_t) bodyLen,
                              NULL);
            urlGetResponse(up);
            elapsed = rGetTicks() - startTime;
            if (recordResults) {
                recordRequest(result, status == 200 || status == 201 || status == 204, elapsed, bodyLen);
            }
        }
        urlClose(up);
        cycle++;
    }
    urlFree(up);

    if (recordResults) {
        calculateStats(result);
        printBenchResult(result);
        saveBenchGroup("mixed", &result, 1);
        freeBenchResult(result);
    }
}

/*
   Run soak test - one complete sweep of all benchmarks
   Warms up server, caches, and allows JIT optimizations
 */
static void runSoakTest(void)
{
    Ticks duration = getSoakDuration();

    tinfo("Soak phase: Warming up all code paths...");
    tinfo("  Running static file requests (%lld secs)...", (long long) duration / TPS);
    benchStaticFiles(duration, false);

    tinfo("  Running HTTPS requests (%lld secs)...", (long long) duration / TPS);
    benchHTTPS(duration, false);

    tinfo("  Running WebSocket requests (%lld secs)...", (long long) duration / TPS);
    benchWebSockets(duration, false);

    tinfo("  Running upload requests (%lld secs)...", (long long) duration / TPS);
    benchUploads(duration, false);

    tinfo("  Running auth requests (%lld secs)...", (long long) duration / TPS);
    benchAuth(duration, false);

    tinfo("  Running action requests (%lld secs)...", (long long) duration / TPS);
    benchActions(duration, false);

    tinfo("  Running mixed workload (%lld secs)...", (long long) duration / TPS);
    benchMixed(duration, false);

    tinfo("Soak phase complete - all code paths warmed");
}

/*
   Test: Benchmark static file serving
 */
static void testBenchmarkStatic(void)
{
    Ticks duration = getBenchDuration();

    tinfo("=== Benchmarking Static File Serving (%lld secs) ===", (long long) duration / TPS);
    benchStaticFiles(duration, true);
}

/*
   Test: Benchmark file uploads
 */
static void testBenchmarkUploads(void)
{
    Ticks duration = getBenchDuration();

    tinfo("=== Benchmarking File Uploads (%lld secs) ===", (long long) duration / TPS);
    benchUploads(duration, true);
}

/*
   Test: Benchmark authenticated routes
 */
static void testBenchmarkAuth(void)
{
    Ticks duration = getBenchDuration();

    tinfo("=== Benchmarking Authentication (%lld secs) ===", (long long) duration / TPS);
    benchAuth(duration, true);
}

/*
   Test: Benchmark HTTPS performance
 */
static void testBenchmarkHTTPS(void)
{
    Ticks duration = getBenchDuration();

    tinfo("=== Benchmarking HTTPS (%lld secs) ===", (long long) duration / TPS);
    benchHTTPS(duration, true);
}

/*
   Test: Benchmark action handlers
 */
static void testBenchmarkActions(void)
{
    Ticks duration = getBenchDuration();

    tinfo("=== Benchmarking Action Handlers (%lld secs) ===", (long long) duration / TPS);
    benchActions(duration, true);
}

/*
   Test: Benchmark mixed workload
 */
static void testBenchmarkMixed(void)
{
    Ticks duration = getBenchDuration();

    tinfo("=== Benchmarking Mixed Workload (%lld secs) ===", (long long) duration / TPS);
    benchMixed(duration, true);
}

/*
   Test: Benchmark WebSocket operations
 */
static void testBenchmarkWebSockets(void)
{
    Ticks duration = getBenchDuration();

    tinfo("=== Benchmarking WebSockets (%lld secs) ===", (long long) duration / TPS);
    benchWebSockets(duration, true);
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

    tinfo("=== Benchmarking Static Files (Raw HTTP) (%lld secs) ===", (long long) duration / TPS);
    benchStaticFilesRaw(duration, true, host, port, false);
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

    tinfo("=== Benchmarking Static Files (Raw HTTPS) (%lld secs) ===", (long long) duration / TPS);
    benchStaticFilesRaw(duration, true, host, port, true);
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

    // Check for TESTME_CLASS to run a single benchmark class
    testClass = getenv("TESTME_CLASS");
    if (testClass && *testClass) {
        // Validate test class
        if (!smatch(testClass, "static") && !smatch(testClass, "https") &&
            !smatch(testClass, "raw_http") && !smatch(testClass, "raw_https") &&
            !smatch(testClass, "uploads") && !smatch(testClass, "auth") &&
            !smatch(testClass, "actions") && !smatch(testClass, "mixed") &&
            !smatch(testClass, "websockets")) {
            tinfo("Error: Invalid TESTME_CLASS='%s'", testClass);
            tinfo("Valid values: static, https, raw_http, raw_https, uploads, auth, actions, mixed, websockets");
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
        tinfo("=== Phase 1: Soak - %s (%lld secs) ===", testClass, (long long) duration / TPS);
        if (smatch(testClass, "static")) {
            benchStaticFiles(duration, false);
        } else if (smatch(testClass, "https")) {
            benchHTTPS(duration, false);
        } else if (smatch(testClass, "raw_http")) {
            host = sclone(HTTP + 7);
            portStr = schr(host, ':');
            if (portStr) {
                *portStr = '\0';
                port = atoi(portStr + 1);
            } else {
                port = 80;
            }
            benchStaticFilesRaw(duration, false, host, port, false);
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
            benchStaticFilesRaw(duration, false, host, port, true);
            rFree(host);
        } else if (smatch(testClass, "uploads")) {
            benchUploads(duration, false);
        } else if (smatch(testClass, "auth")) {
            benchAuth(duration, false);
        } else if (smatch(testClass, "actions")) {
            benchActions(duration, false);
        } else if (smatch(testClass, "mixed")) {
            benchMixed(duration, false);
        } else if (smatch(testClass, "websockets")) {
            benchWebSockets(duration, false);
        }
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
        } else if (smatch(testClass, "uploads")) {
            testBenchmarkUploads();
        } else if (smatch(testClass, "auth")) {
            testBenchmarkAuth();
        } else if (smatch(testClass, "actions")) {
            testBenchmarkActions();
        } else if (smatch(testClass, "mixed")) {
            testBenchmarkMixed();
        } else if (smatch(testClass, "websockets")) {
            testBenchmarkWebSockets();
        }

        // Save results
        tinfo("=== Phase 3: Analysis ===");
        saveFinalResults();

    } else {
        // Run all benchmarks
        // Configure duration from TESTME_DURATION
        // Active test groups: static, HTTPS, raw HTTP, raw HTTPS, uploads, auth, actions, mixed, websockets (9 groups)
        configureDuration(9);

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
        printf("\n");

        // Phase 2: Benchmarks (measurement)
        tinfo("=== Phase 2: Benchmarks ===");
        testBenchmarkStatic();
        testBenchmarkHTTPS();
        testBenchmarkStaticRawHTTP();
        testBenchmarkStaticRawHTTPS();
        testBenchmarkWebSockets();
        testBenchmarkUploads();
        testBenchmarkAuth();
        testBenchmarkActions();
        testBenchmarkMixed();

        // Phase 3: Save results
        tinfo("=== Phase 3: Analysis ===");
        saveFinalResults();
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
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
