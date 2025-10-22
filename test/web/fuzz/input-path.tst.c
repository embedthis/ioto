/*
    input-path.fuzz.c - URL path validation fuzzer

    Fuzzes URL path parsing and validation to find path traversal,
    injection, and sanitization bypass vulnerabilities.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "../test.h"
#include "fuzzLib.h"

/*********************************** Locals ***********************************/

static cchar *HTTP;
static int   fuzzResult = 0;

// Path fuzzing corpus
static cchar *pathCorpus[] = {
    "/",
    "/index.html",
    "/api/users",
    "/path/to/resource",
    "/file.txt",
    "/dir/",
    "/../../../etc/passwd",
    "/./././file",
    "/path/../../../etc/shadow",
    "/%2e%2e%2f%2e%2e%2f",
    "/path/..%2f..%2f",
    "/path%00.txt",
    "/path/file\x00.html",
    "/path//double//slash",
    "/path\\backslash\\win",
    "/very/long/path/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/w/x/y/z",
    "/unicode/\xc3\xa9\xc3\xa0",
    "/special/<script>alert(1)</script>",
    "/sql/';DROP TABLE users--",
    "/null\x00byte",
    "/.git/config",
    "/.env",
    "/.htaccess",
    "/~user/private",
    "/cgi-bin/../../etc/passwd",
    NULL
};

/************************************ Code ************************************/

/*
    Test oracle - returns true if path handled safely
 */
static bool testPathValidation(cchar *fuzzPath, size_t len)
{
    Url  *up;
    char url[2048];
    int  status;

    up = urlAlloc(0);
    urlSetTimeout(up, 2000);

    // Construct URL with fuzzed path
    if (len >= sizeof(url) - slen(HTTP) - 1) {
        // Path too long
        urlFree(up);
        return true;
    }

    snprintf(url, sizeof(url), "%s%.*s", HTTP, (int) len, fuzzPath);

    // Fetch URL and check response
    status = urlFetch(up, "GET", url, NULL, 0, NULL);

    // Acceptable responses:
    // 200 - OK (file exists and is accessible)
    // 301/302 - Redirect (might be normalizing path)
    // 400 - Bad Request (rejected malformed path)
    // 403 - Forbidden (access denied, good security)
    // 404 - Not Found (file doesn't exist)
    // 414 - URI Too Long (rejected oversized path)

    // Unacceptable responses that might indicate vulnerability:
    // 500 - Internal Server Error (crashed during parsing?)
    // Accessing /etc/passwd or other sensitive files

    if (status == 500) {
        tinfo("Internal server error for path: %.*s", (int) len, fuzzPath);
        // This might indicate a vulnerability
        return false;
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
                return false;
            }
        }
    }

    urlFree(up);
    return true;  // Test passed
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
    Fiber main - runs the fuzzer
 */
static void fuzzFiber(void *arg)
{
    FuzzRunner *runner;
    FuzzConfig *config = (FuzzConfig*) arg;
    int        i;

    // Setup test environment
    if (!setup(&HTTP, NULL)) {
        tfail("Cannot setup test environment");
        fuzzResult = -1;
        rStop();
        return;
    }

    tinfo("Starting URL path validation fuzzer");
    tinfo("Target: %s", HTTP);
    tinfo("Iterations: %d", config->iterations);

    // Initialize fuzzer
    runner = fuzzInit(config);
    fuzzSetOracle(runner, testPathValidation);
    fuzzSetMutator(runner, mutatePathInput);

    // Load initial corpus
    for (i = 0; pathCorpus[i]; i++) {
        fuzzAddCorpus(runner, pathCorpus[i], slen(pathCorpus[i]));
    }

    // Load corpus from file if exists
    fuzzLoadCorpus(runner, "corpus/paths.txt");

    // Run fuzzing campaign
    int crashes = fuzzRun(runner);

    // Report results
    fuzzReport(runner);
    fuzzFree(runner);

    fuzzResult = crashes;
    rStop();
}

/*
    Main entry point
 */
int main(int argc, char **argv)
{
    int iterations = getenv("TESTME_ITERATIONS") ? atoi(getenv("TESTME_ITERATIONS")) : 0;

    FuzzConfig config = {
        .iterations = (iterations > 1) ? iterations : 20000,
        .timeout = 5000,
        .parallel = 1,
        .seed = 0,
        .crashDir = "./crashes/input-path",
        .corpusDir = "./corpus/path",
        .verbose = getenv("TESTME_VERBOSE") != NULL,
    };

    // Initialize runtime with fiber
    rInit(fuzzFiber, &config);
    rServiceEvents();
    rTerm();

    if (fuzzResult < 0) {
        return 1;  // Setup failed
    }

    if (fuzzResult > 0) {
        tfail("Found %d path validation issues", fuzzResult);
        return 1;
    }

    tinfo("Path validation fuzzing complete - no issues found");
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
