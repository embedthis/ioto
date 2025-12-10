/*
    url.tst.c - URL path validation fuzzer

    Fuzzes URL path parsing and validation to find path traversal,
    injection, and sanitization bypass vulnerabilities.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "../test.h"
#include "fuzz.h"
#include "fuzz.c"

/*********************************** Config ***********************************/

#define CORPUS_FILE "corpus/url-paths.txt"
#define CRASH_DIR   "crashes/url"

/*********************************** Locals ***********************************/

static char *HTTP;
static int  fuzzResult = 0;

/*********************************** Forwards *********************************/

static void fuzzFiber(void *arg);
static bool testPathValidation(cchar *fuzzPath, size_t len);
static char *mutatePathInput(cchar *input, size_t *len);

/************************************ Code ************************************/
/*
    Main entry point
 */
int main(int argc, char **argv)
{
    int duration = getenv("TESTME_DURATION") ? atoi(getenv("TESTME_DURATION")) : 0;

    FuzzConfig config = {
        .crashDir = CRASH_DIR,
        .duration = (duration > 0) ? duration * 1000 : 60000,
        .iterations = 0,
        .parallel = 1,
        .seed = 0,
        .mutate = getenv("FUZZ_MUTATE") ? atoi(getenv("FUZZ_MUTATE")) : 1,
        .randomize = getenv("FUZZ_RANDOMIZE") ? atoi(getenv("FUZZ_RANDOMIZE")) : 1,
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
        tfail("Found %d path validation issues", fuzzResult);
        return 1;
    }
    tinfo("Path validation fuzzing complete - no issues found\n");
    return 0;
}

/*
    Fiber main - runs the fuzzer
 */
static void fuzzFiber(void *arg)
{
    FuzzRunner *runner;
    FuzzConfig *config = (FuzzConfig*) arg;
    cchar      *replayFile = getenv("FUZZ_REPLAY");

    if (!setup(&HTTP, NULL)) {
        tfail("Cannot setup test environment");
        fuzzResult = -1;
        rStop();
        return;
    }

    // Check if we're replaying a crash file
    if (replayFile) {
        tinfo("Replaying crash file: %s", replayFile);
        tinfo("Target: %s", HTTP);

        config->mutate = 0;
        config->randomize = 0;

        runner = fuzzInit(config);
        fuzzSetOracle(runner, testPathValidation);

        // Load single crash file instead of corpus
        int loaded = fuzzLoadCorpus(runner, replayFile);
        if (loaded == 0) {
            tfail("Failed to load crash file: %s", replayFile);
            fuzzResult = -1;
            rStop();
            return;
        }

        // Run once with the crash input (no mutations)
        config->duration = 0;
        config->iterations = 1;
        tinfo("Running single iteration with crash input...");
    } else {
        tinfo("Starting URL path validation fuzzer");
        tinfo("Target: %s", HTTP);
        if (config->duration > 0) {
            tinfo("Duration: %.1fs", (double) config->duration / 1000.0);
        } else {
            tinfo("Iterations: %d", config->iterations);
        }

        runner = fuzzInit(config);
        fuzzSetOracle(runner, testPathValidation);
        fuzzSetMutator(runner, mutatePathInput);

        fuzzLoadCorpus(runner, CORPUS_FILE);
    }

    int crashes = fuzzRun(runner);

    fuzzReport(runner);
    fuzzFree(runner);

    fuzzResult = crashes;
    rStop();
}

/*
    Test oracle - returns true if path handled safely
 */
static bool testPathValidation(cchar *fuzzPath, size_t len)
{
    Url  *up;
    char url[2048];
    int  status;

    /*
        Use URL_NO_LINGER to prevent TIME_WAIT accumulation during fuzzing
        This sends RST instead of FIN on close (works on Linux, macOS, Windows)
     */
    up = urlAlloc(0);
    urlSetTimeout(up, 2000);

    // Construct URL with fuzzed path
    if (len >= sizeof(url) - slen(HTTP) - 1) {
        // Path too long
        urlFree(up);
        return 1;
    }
    snprintf(url, sizeof(url), "%s%.*s", HTTP, (int) len, fuzzPath);

    // Fetch URL and check response
    status = urlFetch(up, "GET", url, NULL, 0, NULL);

    /*
        Check if server crashed during request
        urlFetch returns error code if connection failed
     */
    if (status < 0) {
        if (!fuzzIsServerAlive(fuzzGetServerPid())) {
            tinfo("Server crashed during path validation: %.*s", (int) len, fuzzPath);
            fuzzReportServerCrash(fuzzPath, len);
            urlFree(up);
            return 0;  // Server crash detected
        }
    }

    /*
        Acceptable responses:
            200 - OK (file exists and is accessible)
            301/302 - Redirect (might be normalizing path)
            400 - Bad Request (rejected malformed path)
            403 - Forbidden (access denied, good security)
            404 - Not Found (file doesn't exist)
            414 - URI Too Long (rejected oversized path)

        Unacceptable responses that might indicate vulnerability:
            500 - Internal Server Error (crashed during parsing?)
            Accessing /etc/passwd or other sensitive files
     */
    if (status == 500) {
        tinfo("Internal server error for path: %.*s", (int) len, fuzzPath);
        // This might indicate a vulnerability
        return 0;
    }

    // Check if we got unexpected file content (like /etc/passwd)
    if (status == 200) {
        cchar *response = urlGetResponse(up);
        if (response) {
            // Check for signs of /etc/passwd
            if (scontains(response, "root:x:0:0") ||
                scontains(response, "daemon:x:1:1")) {
                tinfo("Possible path traversal - got sensitive file content");
                urlFree(up);
                return 0;
            }
        }
    }
    urlFree(up);

    /*
        Check if server is still alive after processing request
        This catches delayed crashes
     */
    if (!fuzzIsServerAlive(fuzzGetServerPid())) {
        tinfo("Server crashed after processing path: %.*s", (int) len, fuzzPath);
        fuzzReportServerCrash(fuzzPath, len);
        return 0; // Server crash detected
    }

    return 1;     // Test passed
}

/*
    Path-specific mutation strategies
 */
static char *mutatePathInput(cchar *input, size_t *len)
{
    int    strategy = random() % 20;
    char   *result;
    size_t newlen, i;

    switch (strategy) {
    case 0:      // Add path traversal
        newlen = *len + 3;
        result = rAlloc(newlen);
        memcpy(result, "../", 3);
        memcpy(result + 3, input, *len);
        *len = newlen;
        return result;

    case 1:      // URL encode some characters
        result = webEncode(input);
        *len = slen(result);
        return result;

    case 2:      // Double URL encode
        result = webEncode(input);
        char *double_encoded = webEncode(result);
        rFree(result);
        *len = slen(double_encoded);
        return double_encoded;

    case 3:      // Add null byte
        newlen = *len + 1;
        result = rAlloc(newlen);
        memcpy(result, input, *len);
        result[*len] = '\0';
        *len = newlen;
        return result;

    case 4:      // Mix slashes
        result = sclone(input);
        for (i = 0; i < *len; i++) {
            if (result[i] == '/' && (random() % 2)) {
                result[i] = '\\';
            }
        }
        return result;

    case 5:      // Duplicate slashes
        result = fuzzReplace(input, len, "/", "//");
        return result;

    case 6:      // Add dots
        result = fuzzReplace(input, len, "/", "/./");
        return result;

    case 7:      // Long path component
        newlen = *len + 500;
        result = rAlloc(newlen);
        memcpy(result, input, *len);
        memset(result + *len, 'A', 500);
        *len = newlen;
        return result;

    case 8:      // Unicode normalization attack
        return fuzzReplace(input, len, ".", "\xc0\xae");

    case 9:      // Overlong UTF-8
        return fuzzReplace(input, len, "/", "\xc0\xaf");

    case 10:     // Windows device names
        return fuzzReplace(input, len, "file", "CON");

    case 11:     // Trailing dots (Windows)
        newlen = *len + 3;
        result = rAlloc(newlen);
        memcpy(result, input, *len);
        memcpy(result + *len, "...", 3);
        *len = newlen;
        return result;

    case 12:      // UNC path
        newlen = *len + 2;
        result = rAlloc(newlen);
        memcpy(result, "//", 2);
        memcpy(result + 2, input, *len);
        *len = newlen;
        return result;

    case 13:      // Case variation
        result = sclone(input);
        for (i = 0; i < *len; i++) {
            if (result[i] >= 'a' && result[i] <= 'z' && (random() % 2)) {
                result[i] = result[i] - 'a' + 'A';
            }
        }
        return result;

    case 14:      // Inject special files
        return fuzzReplace(input, len, "file", ".git/config");

    case 15:      // Bit flip
        return fuzzBitFlip(input, len);

    case 16:      // Truncate
        return fuzzTruncate(input, len);

    case 17:      // Insert random
        return fuzzInsertRandom(input, len);

    case 18:      // Delete random
        return fuzzDeleteRandom(input, len);

    case 19:      // Splice
        return fuzzSplice(input, len);

    default:
        return sclone(input);
    }
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
