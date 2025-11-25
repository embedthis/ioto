/*
    leak.tst.c - Memory leak detection test for URL module (client API only)

    This test iterates over different classes of HTTP requests and monitors memory usage
    to detect memory leaks in the URL HTTP client library. Memory usage is sampled
    periodically and checked for stability.

    SCOPE:
        Tests ONLY the URL module client-side API (urlFetch, urlAlloc, urlFree, etc.)
        The embedded web server is test infrastructure only and is NOT being tested.

    USAGE:
        tm leak                    # Run leak test with default iterations (250,000 per test)
        tm --iterations 1000 leak  # Run with custom iteration count
        tm -s leak                 # Show compilation and test details

    DESCRIPTION:
        This is a manual-only test that runs extensive iterations of different HTTP request
        types while monitoring memory usage. Each test class includes:
        - Soak-in period (default 5,000 iterations) to stabilize memory allocations
        - Test period (default 250,000 iterations) with periodic memory sampling
        - Memory growth analysis (15% threshold)

    ITERATION SCALING:
        When using tm --iterations N:
        - If N > 1: TEST_ITERATIONS = N, SOAK_ITERATIONS = max(100, N/50)
        - If N = 1: Use compiled defaults (soak: 5,000, test: 250,000)
        - No --iterations: Use compiled defaults

    TEST CLASSES (CLIENT-SIDE OPERATIONS):
        - Basic GET requests
        - POST requests with form data
        - Basic authentication
        - Digest authentication (SHA-256)
        - Chunked transfer encoding
        - Custom headers
        - Streaming reads
        - Keep-alive connections

    PLATFORMS:
        - Mac and Linux only (uses getrusage for memory monitoring)
        - Not supported on Windows

    NOTE:
        Memory usage can be unstable initially and takes time to stabilize (soak-in period).
        The test allows 15% memory growth to account for normal runtime variance.
        This test measures only the client process memory usage, not the server.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "../test.h"

#if !WINDOWS
#include <sys/resource.h>
#endif

/********************************** Forwards **********************************/

#ifndef max
    #define max(a,b) ((a) > (b) ? (a) : (b))
#endif

static size_t getCurrentMemoryUsage();
static void runBasicGET(cchar *http, int iterations);
static void runPOST(cchar *http, int iterations);
static void runBasicAuth(cchar *http, int iterations);
static void runDigestAuth(cchar *http, int iterations);
static void runChunked(cchar *http, int iterations);
static void runHeaders(cchar *http, int iterations);
static void runStreaming(cchar *http, int iterations);
static void runKeepAlive(cchar *http, int iterations);

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

// Memory leak detection parameters (defaults)
#define DEFAULT_SOAK_ITERATIONS 5000       // Initial iterations to stabilize memory (do twice)
#define DEFAULT_TEST_ITERATIONS 250000     // Iterations per test class
#define SAMPLE_INTERVAL 50                 // Sample memory every N iterations
#define MAX_SAMPLES 100                    // Maximum memory samples to collect
#define LEAK_THRESHOLD 1.15                // 15% memory growth threshold (allows for runtime variance)

// Runtime iterations (set from TESTME_ITERATIONS or defaults)
static int SOAK_ITERATIONS = DEFAULT_SOAK_ITERATIONS;
static int TEST_ITERATIONS = DEFAULT_TEST_ITERATIONS;

/************************************* Code ************************************/

/*
    Get current memory usage in bytes (RSS - Resident Set Size)
    Returns 0 on error or unsupported platform
 */
static size_t getCurrentMemoryUsage()
{
#if !WINDOWS
    struct rusage usage;

    if (getrusage(RUSAGE_SELF, &usage) == 0) {
#if MACOSX
        // macOS returns RSS in bytes
        return (size_t)usage.ru_maxrss;
#else
        // Linux returns RSS in kilobytes
        return (size_t)usage.ru_maxrss * 1024;
#endif
    }
#endif
    return 0;
}

/*
    Run basic GET requests
 */
static void runBasicGET(cchar *http, int iterations)
{
    Url     *up;
    char    url[256];
    int     i, status;

    for (i = 0; i < iterations; i++) {
        up = urlAlloc(0);
        status = urlFetch(up, "GET", SFMT(url, "%s/test/index.html", http), NULL, 0, NULL);
        if (status == 200) {
            urlGetResponse(up);
        }
        urlFree(up);
    }
}

/*
    Run POST requests
 */
static void runPOST(cchar *http, int iterations)
{
    Url     *up;
    char    url[256];
    char    data[1024];
    int     i, status;

    // Prepare test data
    sfmt(data, sizeof(data), "name=leak_test&value=%d&data=", rand());
    for (i = slen(data); i < (int)sizeof(data) - 1; i++) {
        data[i] = 'A' + (i % 26);
    }
    data[sizeof(data) - 1] = '\0';

    for (i = 0; i < iterations; i++) {
        up = urlAlloc(0);
        status = urlFetch(up, "POST", SFMT(url, "%s/test/", http), data, slen(data),
                         "Content-Type: application/x-www-form-urlencoded");
        if (status == 200) {
            urlGetResponse(up);
        }
        urlFree(up);
    }
}

/*
    Run Basic authentication requests
 */
static void runBasicAuth(cchar *http, int iterations)
{
    Url     *up;
    char    url[256];
    int     i, status;

    for (i = 0; i < iterations; i++) {
        up = urlAlloc(0);
        urlSetAuth(up, "bob", "password", "basic");
        status = urlFetch(up, "GET", SFMT(url, "%s/basic/secret.html", http), NULL, 0, NULL);
        if (status == 200) {
            urlGetResponse(up);
        }
        urlFree(up);
    }
}

/*
    Run Digest authentication requests
 */
static void runDigestAuth(cchar *http, int iterations)
{
    Url     *up;
    char    url[256];
    int     i, status;

    for (i = 0; i < iterations; i++) {
        up = urlAlloc(0);
        urlSetAuth(up, "alice", "password", "digest");
        status = urlFetch(up, "GET", SFMT(url, "%s/digest/secret.html", http), NULL, 0, NULL);
        if (status == 200) {
            urlGetResponse(up);
        }
        urlFree(up);
    }
}

/*
    Run chunked transfer encoding requests
 */
static void runChunked(cchar *http, int iterations)
{
    Url     *up;
    char    url[256];
    int     i, status;

    for (i = 0; i < iterations; i++) {
        up = urlAlloc(0);
        status = urlFetch(up, "GET", SFMT(url, "%s/test/chunked", http), NULL, 0, NULL);
        if (status == 200) {
            urlGetResponse(up);
        }
        urlFree(up);
    }
}

/*
    Run requests with custom headers
 */
static void runHeaders(cchar *http, int iterations)
{
    Url     *up;
    char    url[256];
    int     i, status;

    for (i = 0; i < iterations; i++) {
        up = urlAlloc(0);
        status = urlFetch(up, "GET", SFMT(url, "%s/test/index.html", http), NULL, 0,
                         "X-Custom-Header: test-value\r\nX-Test-ID: %d", i);
        if (status == 200) {
            urlGetResponse(up);
        }
        urlFree(up);
    }
}

/*
    Run streaming reads
 */
static void runStreaming(cchar *http, int iterations)
{
    Url     *up;
    char    url[256];
    char    buffer[4096];
    int     i, status;
    ssize   len;

    for (i = 0; i < iterations; i++) {
        up = urlAlloc(0);
        status = urlFetch(up, "GET", SFMT(url, "%s/test/index.html", http), NULL, 0, NULL);
        if (status == 200) {
            while ((len = urlRead(up, buffer, sizeof(buffer))) > 0) {
                // Read and discard
            }
        }
        urlFree(up);
    }
}

/*
    Run keep-alive connection requests
 */
static void runKeepAlive(cchar *http, int iterations)
{
    Url     *up;
    char    url[256];
    int     i, status;

    // Multiple requests on same connection
    up = urlAlloc(0);
    for (i = 0; i < iterations; i++) {
        status = urlFetch(up, "GET", SFMT(url, "%s/test/index.html", http), NULL, 0, NULL);
        if (status == 200) {
            urlGetResponse(up);
        }
    }
    urlFree(up);
}

static bool leakSetup(char **HTTP, char **HTTPS)
{
    Json    *json;
    
    rSetSocketDefaultCerts("../../certs/roots.crt", 0, 0, 0);
    urlSetDefaultTimeout(30 * TPS);

    json = jsonParseFile("../web.json5", NULL, 0);
    *HTTP = jsonGetClone(json, 0, "web.listen[0]", NULL);
    if (!*HTTP) {
        tfail("Cannot get HTTP from web.json5");
        return 0;
    }
    *HTTPS = jsonGetClone(json, 0, "web.listen[1]", NULL);
    if (!*HTTPS) {
        tfail("Cannot get HTTPS from web.json5");
        return 0;
    }
    jsonFree(json);
    return 1;
}

/*
    Test class definition
 */
typedef struct {
    cchar   *name;
    void    (*fn)(cchar *http, int iterations);
} TestClass;

/*
    Configure iterations from TESTME_ITERATIONS environment variable
    If TESTME_ITERATIONS is set and != 1, use that value.
    Otherwise use the compiled-in defaults.
 */
static void configureIterations()
{
    cchar *iterStr;
    int iterations;

    // SECURITY Acceptable: Using getenv for test configuration only
    iterStr = getenv("TESTME_ITERATIONS");
    if (iterStr) {
        iterations = atoi(iterStr);
        if (iterations > 1) {
            /*
                Scale iterations: use provided value for TEST_ITERATIONS
                and 1/50th for SOAK_ITERATIONS (min 100)
             */
            TEST_ITERATIONS = iterations;
            SOAK_ITERATIONS = max(100, iterations / 50);
            tinfo("Using TESTME_ITERATIONS: %d (soak: %d, test: %d)",
                  iterations, SOAK_ITERATIONS, TEST_ITERATIONS);
        }
    }
}

/*
    Main fiber entry point
 */
static void fiberMain(void *data)
{
#if WINDOWS
    tskip("Leak test not supported on Windows");
#else
    configureIterations();

    if (leakSetup(&HTTP, &HTTPS)) {
        TestClass tests[] = {
            { "Basic GET", runBasicGET },
            { "POST", runPOST },
            { "Basic Auth", runBasicAuth },
            { "Digest Auth", runDigestAuth },
            { "Chunked Encoding", runChunked },
            { "Custom Headers", runHeaders },
            { "Streaming Reads", runStreaming },
            { "Keep-Alive", runKeepAlive },
        };
        int numTests = sizeof(tests) / sizeof(tests[0]);
        int totalRequests = numTests * (SOAK_ITERATIONS + TEST_ITERATIONS);
        size_t startMem, soakMem, baselineMem, testStartMem, testEndMem, endMem;
        Ticks startTime, endTime;
        int i;

        tinfo("=== URL Module Memory Leak Test Suite (Client API Only) ===");
        tinfo("This test runs multiple request types to detect memory leaks in the URL client");
        tinfo("Leak threshold: %.0f%% memory growth", (LEAK_THRESHOLD - 1.0) * 100);
        tinfo("Note: Tests client-side memory only; server is test infrastructure");
        tinfo("");

        // Capture starting state
        startMem = getCurrentMemoryUsage();
        startTime = rGetTicks();
        tinfo("Test starting: %zu bytes memory", startMem);
        tinfo("Total requests: %d (%d tests × %d iterations each)",
              totalRequests, numTests, SOAK_ITERATIONS + TEST_ITERATIONS);
        tinfo("");

        /*
            Phase 1: Soak-in period - run all test classes to stabilize memory
            Do twice so we rotate through all requests 
         */
        tinfo("=== SOAK-IN PHASE (%d iterations per test) ===", SOAK_ITERATIONS);
        for (i = 0; i < numTests; i++) {
            tinfo("Soak-in: %s...", tests[i].name);
            tests[i].fn(HTTP, SOAK_ITERATIONS);
        }
        for (i = 0; i < numTests; i++) {
            tinfo("Soak-in: %s...", tests[i].name);
            tests[i].fn(HTTP, SOAK_ITERATIONS);
        }
        soakMem = getCurrentMemoryUsage();
        double soakGrowth = (double)soakMem / (double)startMem;
        double soakGrowthPercent = (soakGrowth - 1.0) * 100.0;
        tinfo("Soak-in complete: %zu bytes (growth: %.2fx, %.1f%% from start)",
              soakMem, soakGrowth, soakGrowthPercent);
        tinfo("");

        // Get baseline memory after soak-in
        baselineMem = soakMem;
        tinfo("=== TEST PHASE (%d iterations per test) ===", TEST_ITERATIONS);
        tinfo("Baseline memory: %zu bytes", baselineMem);
        tinfo("");

        // Phase 2: Run each test class and measure memory growth
        for (i = 0; i < numTests; i++) {
            testStartMem = getCurrentMemoryUsage();
            tinfo("Running: %s (%d iterations)...", tests[i].name, TEST_ITERATIONS);
            tests[i].fn(HTTP, TEST_ITERATIONS);
            testEndMem = getCurrentMemoryUsage();

            double classGrowth = (double)testEndMem / (double)testStartMem;
            double classGrowthPercent = (classGrowth - 1.0) * 100.0;
            /*
            tinfo("  %s: %zu → %zu bytes (%.2fx, %.1f%%)",
                  tests[i].name, testStartMem, testEndMem, classGrowth, classGrowthPercent);
            */

            // Check for leak in this test class
            ttrue(classGrowth < LEAK_THRESHOLD,
                  "%s: Memory growth %.2fx (%.1f%%) vs threshold %.2fx (start: %zu, end: %zu)",
                  tests[i].name, classGrowth, classGrowthPercent, LEAK_THRESHOLD,
                  testStartMem, testEndMem);
        }

        // Calculate final statistics
        endTime = rGetTicks();
        endMem = getCurrentMemoryUsage();
        double durationSecs = (double)(endTime - startTime) / (double)TPS;
        double overallGrowth = (double)endMem / (double)baselineMem;
        double overallGrowthPercent = (overallGrowth - 1.0) * 100.0;
        double requestsPerSec = totalRequests / durationSecs;

        tinfo("");
        tinfo("=== FINAL SUMMARY ===");
        tinfo("Duration:            %.1f seconds (%.1f minutes)", durationSecs, durationSecs / 60.0);
        tinfo("Total Requests:      %d", totalRequests);
        tinfo("Requests/Second:     %.1f", requestsPerSec);
        tinfo("Memory:              Baseline: %zu bytes, Final: %zu bytes", baselineMem, endMem);
        tinfo("Overall Growth:      %.2fx (%.1f%%)", overallGrowth, overallGrowthPercent);
        tinfo("Leak Threshold:      %.2fx (%.0f%%)", LEAK_THRESHOLD, (LEAK_THRESHOLD - 1.0) * 100);
        if (overallGrowth < LEAK_THRESHOLD) {
            tinfo("Result:              PASS - No memory leaks detected");
        } else {
            tinfo("Result:              WARNING - Memory growth exceeds threshold");
        }
        tinfo("=== Leak Test Complete ===");
    }
    rFree(HTTP);
    rFree(HTTPS);
#endif
    rStop();
}

/*
    Main entry point
 */
int main(void)
{
    // Seed random number generator for test data
    srand((uint)time(NULL));

    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
