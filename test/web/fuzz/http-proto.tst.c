/*
    http.tst.c - HTTP protocol fuzzer

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "../test.h"
#include "fuzz.h"
#include "fuzz.c"

/*********************************** Config ***********************************/

#define CORPUS_FILE "corpus/http-requests.txt"
#define CRASH_DIR   "crashes/http"

/*********************************** Locals ***********************************/

static cchar      *HTTP;
static FuzzRunner *runner;
static int        fuzzResult = 0;
static bool       serverWasReachable = 0;   // Track if we ever connected successfully
static cchar      *currentFuzzInput = NULL; // Current fuzz input for crash reporting
static size_t     currentFuzzLen = 0;       // Current fuzz input length
static bool       serverCrashed = 0;        // Flag to stop fuzzing when server crashes

/*********************************** Forwards *********************************/

static void fuzzFiber(void *arg);
static bool testHTTPRequest(cchar *request, size_t len);
static char *mutateHTTPRequest(cchar *input, size_t *len);
static bool shouldStopFuzzing(void);

/************************************* Code ***********************************/

int main(int argc, char **argv)
{
    int duration = getenv("TESTME_DURATION") ? atoi(getenv("TESTME_DURATION")) : 0;

    FuzzConfig config = {
        .crashDir = CRASH_DIR,
        .duration = (duration > 0) ? duration * 1000 : 60000,
        .iterations = 0,
        .parallel = 1,
        .mutate = getenv("FUZZ_MUTATE") ? atoi(getenv("FUZZ_MUTATE")) : 1,
        .randomize = getenv("FUZZ_RANDOMIZE") ? atoi(getenv("FUZZ_RANDOMIZE")) : 1,
        .seed = 0,
        .stop = getenv("TESTME_STOP") ? atoi(getenv("TESTME_STOP")) : 0,
        .timeout = 5000,
        .verbose = getenv("TESTME_VERBOSE") != NULL,
    };

    // Extra room for ASAN
    rSetFiberStackSize(256 * 1024);
    rInit(fuzzFiber, &config);
    rServiceEvents();
    rTerm();

    if (fuzzResult < 0) {
        return 1;  // Setup failed
    } else if (fuzzResult > 0) {
        rPrintf("\n✗ Found %d crashes\n", fuzzResult);
        return 1;
    }
    rPrintf("✓ HTTP fuzzing complete - no crashes found\n");
    return 0;
}

static void fuzzFiber(void *arg)
{
    FuzzConfig *config = (FuzzConfig*) arg;
    cchar      *replayFile = getenv("FUZZ_REPLAY");

    // Setup test environment and get HTTP URL from web.json5
    if (!setup((char**) &HTTP, NULL)) {
        rPrintf("Cannot setup test environment\n");
        fuzzResult = -1;
        rStop();
        return;
    }

    // Check if we're replaying a crash file
    if (replayFile) {
        rPrintf("Replaying crash file: %s\n", replayFile);
        rPrintf("Target: %s\n", HTTP);

        config->mutate = 0;
        config->randomize = 0;

        runner = fuzzInit(config);
        fuzzSetOracle(runner, testHTTPRequest);

        // Load single crash file instead of corpus
        int loaded = fuzzLoadCorpus(runner, replayFile);
        if (loaded == 0) {
            rPrintf("✗ Failed to load crash file: %s\n", replayFile);
            fuzzResult = -1;
            rStop();
            return;
        }

        // Run once with the crash input (no mutations)
        config->iterations = 1;
        rPrintf("Running single iteration with crash input...\n");
    } else {
        rPrintf("Starting HTTP protocol fuzzer\n");
        rPrintf("Target: %s\n", HTTP);
        rPrintf("Iterations: %d\n", config->iterations);

        runner = fuzzInit(config);
        fuzzSetOracle(runner, testHTTPRequest);
        fuzzSetMutator(runner, mutateHTTPRequest);
        fuzzSetShouldStopCallback(shouldStopFuzzing);

        // Load corpus from external file
        fuzzLoadCorpus(runner, CORPUS_FILE);
    }

    int crashes = fuzzRun(runner);

    fuzzReport(runner);
    fuzzFree(runner);

    fuzzResult = crashes;
    rStop();
}

/*
    Callback to check if fuzzing should stop
    Returns true if server crashed or if stop-on-error is enabled and any crash was found
 */
static bool shouldStopFuzzing(void)
{
    // Always stop if server crashed (can't continue)
    if (serverCrashed) {
        return true;
    }
    // If stop-on-error is enabled, stop if we found any crashes
    if (runner->config.stop && runner->stats.crashes > 0) {
        return true;
    }

    return false;
}

/*
    Check if server is alive and report crash if dead
    Returns 1 if server is alive, 0 if server crashed
 */
static bool checkServerAlive(cchar *context)
{
    if (!fuzzIsServerAlive(fuzzGetServerPid())) {
        tinfo("Server crashed %s", context);
        fuzzReportServerCrash(currentFuzzInput, currentFuzzLen);
        serverCrashed = 1; // Set flag to stop fuzzing
        return 0;          // Server crash detected
    }
    return 1;              // Server is alive
}

/*
    Inner test function that performs the HTTP request
    Returns:
        1 = success (test passed)
        0 = failure (potential issue or crash)
       -1 = connection failed (needs server alive check)
 */
static int testHTTPRequestInner(cchar *fuzzInput, size_t len)
{
    RSocket *sock;
    cchar   *scheme = NULL, *host = NULL, *path = NULL;
    char    *buf, response[4096];
    ssize   rc;
    int     port;

    port = 0;
    buf = webParseUrl(HTTP, &scheme, &host, &port, &path, NULL, NULL);
    if (!host) {
        host = "localhost";
    }
    sock = rAllocSocket();
    rSetSocketLinger(sock, 0);

    if (rConnectSocket(sock, host, port, -1) < 0) {
        rError("http.tst", "Failed to connect to socket: %s", rGetSocketError(sock));
        rFreeSocket(sock);
        rFree(buf);
        if (serverWasReachable) {
            return 0; // Server stopped responding (potential hang)
        }
        return -1;    // Connection failed - need to check if server is alive
    }
    serverWasReachable = 1;

    rc = rWriteSocket(sock, fuzzInput, len, rGetTicks() + 500);
    if (rc < 0) {
        // Write failure could indicate server crash or connection closed
        rFreeSocket(sock);
        rFree(buf);
        return -1;  // Write failed - need to check if server is alive
    }

    /*
        Read first part of response
     */
    rc = rReadSocket(sock, response, sizeof(response) - 1, rGetTicks());
    if (rc < 0) {
        // Read error - could indicate server crash mid-response
        rFreeSocket(sock);
        rFree(buf);
        return -1;  // Read failed - need to check if server is alive
    }
    response[rc] = '\0';

    /*
        Basic validation: check if response looks like HTTP
        Valid HTTP responses start with "HTTP/"
     */
    if (rc >= 5 && memcmp(response, "HTTP/", 5) != 0) {
        // Server returned non-HTTP response - potential parsing bug
        rFreeSocket(sock);
        rFree(buf);
        return -1; // Protocol error - need to check if server is alive
    }
    rFreeSocket(sock);
    rFree(buf);
    return 1;      // Test passed - server handled request correctly
}

/*
    Outer wrapper that handles server alive checks
 */
static bool testHTTPRequest(cchar *fuzzInput, size_t len)
{
    int result;

    // If server already crashed, stop fuzzing immediately
    if (serverCrashed) {
        return 1;  // Return success to stop recording additional crashes
    }

    // Store current fuzz input for crash reporting
    currentFuzzInput = fuzzInput;
    currentFuzzLen = len;

    result = testHTTPRequestInner(fuzzInput, len);

    if (result == -1) {
        // Inner function returned error - check if server is alive
        return checkServerAlive("during fuzzing");

    } else if (result == 0) {
        // Inner function detected potential issue - return failure
        return 0;
    }
    // Success - do final check if server is still alive after processing
    return checkServerAlive("after processing request");
}

static char *mutateHTTPRequest(cchar *input, size_t *len)
{
    int strategy = random() % 40;  // Expanded to 40 strategies

    switch (strategy) {
    // Generic bit/byte level mutations (10%)
    case 0: return fuzzBitFlip(input, len);
    case 1: return fuzzByteFlip(input, len);
    case 2: return fuzzInsertRandom(input, len);
    case 3: return fuzzDeleteRandom(input, len);

    // HTTP method mutations (12.5%)
    case 4: return fuzzReplace(input, len, "GET", "XGET");
    case 5: return fuzzReplace(input, len, "POST", "XXPOST");
    case 6: return fuzzReplace(input, len, "GET", "GET ");
    case 7: return fuzzReplace(input, len, "GET", "G\x00T");
    case 8: return fuzzReplace(input, len, "GET", "get");  // Case variation

    // HTTP version mutations (10%)
    case 9: return fuzzReplace(input, len, "HTTP/1.1", "HTTP/9.9");
    case 10: return fuzzReplace(input, len, "HTTP/1.1", "HTTP/1.1 ");
    case 11: return fuzzReplace(input, len, "HTTP/1.1", "HTTP/1.2");
    case 12: return fuzzReplace(input, len, "HTTP/1.0", "HTTP/0.9");

    // Line ending mutations (10%)
    case 13: return fuzzReplace(input, len, "\r\n", "\n");       // LF only
    case 14: return fuzzReplace(input, len, "\r\n", "\r");       // CR only
    case 15: return fuzzReplace(input, len, "\r\n", "\r\n\r\n"); // Double CRLF
    case 16: return fuzzReplace(input, len, "\r\n\r\n", "\r\n"); // Remove separator

    // Header delimiter mutations (7.5%)
    case 17: return fuzzReplace(input, len, ": ", ":");          // No space after colon
    case 18: return fuzzReplace(input, len, ": ", ":  ");        // Extra space
    case 19: return fuzzReplace(input, len, "Host:", "Host :");  // Space before colon

    // Content-Length mutations (7.5%)
    case 20: return fuzzReplace(input, len, "Content-Length: 0", "Content-Length: -1");
    case 21: return fuzzReplace(input, len, "Content-Length: 0", "Content-Length: 999999");
    case 22: return fuzzReplace(input, len, "Content-Length: ", "Content-Length: 0");

    // Header name mutations (7.5%)
    case 23: return fuzzReplace(input, len, "Host", "X-Host");
    case 24: return fuzzReplace(input, len, "Content-Type", "Content-Type\x00");
    case 25: return fuzzReplace(input, len, "Accept", "Accept\r\nAccept");

    // Path/URI mutations (10%)
    case 26: return fuzzReplace(input, len, "/", "//");
    case 27: return fuzzReplace(input, len, "/test/", "/test/../test/");
    case 28: return fuzzReplace(input, len, " HTTP", "%20HTTP");
    case 29: return fuzzReplace(input, len, "?", "%3F");

    // Special character injection (7.5%)
    case 30: return fuzzInsertSpecial(input, len);
    case 31: return fuzzReplace(input, len, "Host: localhost", "Host: localhost\x00.evil.com");
    case 32: return fuzzReplace(input, len, "localhost", "local\nhost");

    // Structural mutations (7.5%)
    case 33: return fuzzDuplicate(input, len);
    case 34: return fuzzTruncate(input, len);
    case 35: return fuzzOverwriteRandom(input, len);

    // Chunked encoding mutations (5%)
    case 36: return fuzzReplace(input, len, "0\r\n\r\n", "FFFF\r\n\r\n");
    case 37: return fuzzReplace(input, len, "Transfer-Encoding: chunked",
                                "Transfer-Encoding: chunked\r\nTransfer-Encoding: identity");

    // Body/header separator mutations (5%)
    case 38: return fuzzReplace(input, len, "\r\n\r\n", "\r\n \r\n"); // Space continuation
    case 39: return fuzzReplace(input, len, "\r\n\r\n", "\r\n");      // Remove separator

    default: return sclone(input);
    }
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
