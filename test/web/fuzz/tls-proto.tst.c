/*
    tls.tst.c - TLS/HTTPS protocol fuzzer

    Fuzzes TLS handshake and HTTPS requests to find TLS-specific vulnerabilities
    including handshake errors, certificate validation issues, and cipher suite handling.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "../test.h"
#include "fuzz.h"
#include "fuzz.c"

/*********************************** Config ***********************************/

#define CORPUS_FILE "corpus/tls-requests.txt"
#define CRASH_DIR   "crashes/tls"

/*********************************** Locals ***********************************/

typedef struct {
    bool verifyPeer;
    bool verifyIssuer;
    cchar *ciphers;
    cchar *caPath;
    cchar *hostname;
} TlsConfig;

static cchar      *HTTP;
static cchar      *HTTPS;
static char       *tlsHost;
static int        tlsPort;
static FuzzRunner *runner;
static int        fuzzResult = 0;
static bool       serverWasReachable = 0;
static cchar      *currentFuzzInput = NULL;
static size_t     currentFuzzLen = 0;
static bool       serverCrashed = 0;

/*********************************** Forwards *********************************/

static void fuzzFiber(void *arg);
static bool testTLSRequest(cchar *request, size_t len);
static char *mutateTLSRequest(cchar *input, size_t *len);
static bool shouldStopFuzzing(void);

/************************************* Code ***********************************/

int main(int argc, char **argv)
{
    int duration = getenv("TESTME_DURATION") ? atoi(getenv("TESTME_DURATION")) : 0;

    /*
        Workaround for macOS ASAN bug with pthread_once in getaddrinfo:
        Call getaddrinfo() in main() before any fiber context is created.
        This initializes the pthread_once state while we're still in normal thread context.
     */
    {
        struct addrinfo hints = { 0 }, *result = NULL;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        getaddrinfo("localhost", "443", &hints, &result);
        if (result) {
            freeaddrinfo(result);
        }
    }

    FuzzConfig config = {
        .crashDir = CRASH_DIR,
        .duration = (duration > 0) ? duration * 1000 : 60000,
        .iterations = 0,
        .parallel = 1,
        .mutate = getenv("FUZZ_MUTATE") ? atoi(getenv("FUZZ_MUTATE")) : 1,
        .randomize = getenv("FUZZ_RANDOMIZE") ? atoi(getenv("FUZZ_RANDOMIZE")) : 1,
        .seed = 0,
        .stop = getenv("TESTME_STOP") ? atoi(getenv("TESTME_STOP")) : 1,
        .timeout = 5000,
        .verbose = getenv("TESTME_VERBOSE") != NULL,
    };

    // Extra room for ASAN
    rSetFiberStackSize(256 * 1024);
    rInit(fuzzFiber, &config);
    rServiceEvents();
    rTerm();

    if (fuzzResult < 0) {
        return 1;
    } else if (fuzzResult > 0) {
        rPrintf("\n✗ Found %d crashes\n", fuzzResult);
        return 1;
    }
    rPrintf("✓ TLS fuzzing complete - no crashes found\n");
    return 0;
}

static void fuzzFiber(void *arg)
{
    FuzzConfig *config = (FuzzConfig*) arg;
    cchar      *replayFile = getenv("FUZZ_REPLAY");
    cchar      *scheme = NULL, *path = NULL;
    cchar      *tempHost;
    char       *buf;

    if (!setup((char**) &HTTP, (char**) &HTTPS)) {
        rPrintf("Cannot setup test environment\n");
        fuzzResult = -1;
        rStop();
        return;
    }

    /*
        Parse HTTPS URL to extract host and port for raw socket connection
     */
    tlsPort = 0;
    tempHost = NULL;
    buf = webParseUrl(HTTPS, &scheme, &tempHost, &tlsPort, &path, NULL, NULL);

    /*
        Clone hostname before freeing buffer
     */
    if (!tempHost || *tempHost == '\0') {
        tlsHost = sclone("localhost");
    } else {
        tlsHost = sclone(tempHost);
    }
    if (tlsPort == 0) {
        tlsPort = 4243;
    }
    rFree(buf);

    if (replayFile) {
        rPrintf("Replaying crash file: %s\n", replayFile);
        rPrintf("Target: %s\n", HTTPS);

        config->mutate = 0;
        config->randomize = 0;

        runner = fuzzInit(config);
        fuzzSetOracle(runner, testTLSRequest);

        int loaded = fuzzLoadCorpus(runner, replayFile);
        if (loaded == 0) {
            rPrintf("✗ Failed to load crash file: %s\n", replayFile);
            fuzzResult = -1;
            rStop();
            return;
        }

        config->duration = 0;
        config->iterations = 1;
        rPrintf("Running single iteration with crash input...\n");
    } else {
        rPrintf("Starting TLS protocol fuzzer\n");
        rPrintf("Target: %s\n", HTTPS);
        if (config->duration > 0) {
            rPrintf("Duration: %.1fs\n", (double) config->duration / 1000.0);
        } else {
            rPrintf("Iterations: %d\n", config->iterations);
        }

        runner = fuzzInit(config);
        fuzzSetOracle(runner, testTLSRequest);
        fuzzSetMutator(runner, mutateTLSRequest);
        fuzzSetShouldStopCallback(shouldStopFuzzing);
        fuzzLoadCorpus(runner, CORPUS_FILE);
    }
    int crashes = fuzzRun(runner);

    fuzzReport(runner);
    fuzzFree(runner);
    fuzzResult = crashes;
    rStop();
}

static bool shouldStopFuzzing(void)
{
    if (serverCrashed) {
        return true;
    }
    if (runner->config.stop && runner->stats.crashes > 0) {
        return true;
    }
    return false;
}

static bool checkServerAlive(cchar *context)
{
    if (!fuzzIsServerAlive(fuzzGetServerPid())) {
        tinfo("Server crashed %s", context);
        fuzzReportServerCrash(currentFuzzInput, currentFuzzLen);
        serverCrashed = 1;
        return 0;
    }
    return 1;
}

/*
    Generate random TLS configuration for fuzzing
 */
static void randomTlsConfig(TlsConfig *config)
{
    int choice = random() % 10;

    /*
        Set defaults first
     */
    config->caPath = "../../certs/ca.crt";
    config->verifyPeer = true;
    config->verifyIssuer = true;
    config->ciphers = NULL;
    config->hostname = tlsHost;

    // TODO - vary ciphers
    /*
        Vary TLS configuration 20% of the time 80% use default safe configuration
     */
    if (choice < 2) {
        /*
            Test with different certificate verification modes
         */
        config->verifyPeer = (random() % 2) ? true : false;
        config->verifyIssuer = (random() % 2) ? true : false;
    } else if (choice == 2) {
        /*
            Test with invalid certificate path (will fail verification)
         */
        config->caPath = "/nonexistent/ca.crt";
        config->verifyPeer = false;
        config->verifyIssuer = false;
    } else if (choice == 3) {
        /*
            Test with SNI hostname mismatch
         */
        cchar *hostnames[] = { "evil.com", "*.localhost", "localhost.", "127.0.0.1" };
        config->hostname = hostnames[random() % 4];
        config->verifyPeer = false;
        config->verifyIssuer = false;
    }
}

/*
    Test TLS request using raw rSocket API
    This directly controls TLS handshake and configuration
    Returns:
        1 = success (test passed)
        0 = failure (potential issue or crash)
       -1 = connection failed (needs server alive check)
 */
static int testTLSRequestInner(cchar *fuzzInput, size_t len)
{
    RSocket   *sock;
    TlsConfig config = { 0 };
    char      response[4096];
    ssize     rc;
    Ticks     deadline;

    /*
        Generate random TLS configuration for this test
     */
    randomTlsConfig(&config);

    /*
        Validate inputs
     */
    if (!tlsHost || !config.hostname) {
        return -1;
    }

    /*
        Allocate and configure socket
     */
    sock = rAllocSocket();
    if (!sock) {
        return -1;
    }
    rSetSocketLinger(sock, 0);

    /*
        Enable TLS on socket (must be before rConnectSocket)
     */
    rSetTls(sock);

    /*
        Configure TLS parameters
        This is where we can fuzz certificate validation, ciphers, etc.
     */
    rSetSocketCerts(sock, config.caPath, NULL, NULL, NULL);
    rSetSocketVerify(sock, config.verifyPeer, config.verifyIssuer);
    if (config.ciphers) {
        rSetSocketCiphers(sock, config.ciphers);
    }
    deadline = rGetTicks() + 20 * TPS;

    /*
        Connect and perform TLS handshake
        This tests:
        - TCP connection
        - TLS handshake (ClientHello, ServerHello, etc.)
        - Certificate validation
        - Cipher negotiation
     */
    serverWasReachable = 1;
    rc = rConnectSocket(sock, config.hostname, tlsPort, deadline);
    if (rc < 0) {
        rTrace("tls.tst", "Failed to connect to socket: %s", rGetSocketError(sock));
        rFreeSocket(sock);
        /*
            Connection/TLS handshake failure is often expected during fuzzing:
            - SNI hostname mismatch
            - Invalid certificates
            - Malformed TLS handshake
            Just check if server is still alive
         */
        return -1;
    }
    serverWasReachable = 1;

    /*
        Send raw HTTP request over established TLS connection
        The fuzzInput contains raw HTTP request bytes
     */
    if (len > 0 && fuzzInput) {
        rc = rWriteSocket(sock, fuzzInput, len, deadline);
        if (rc < 0) {
            rFreeSocket(sock);
            return -1;
        }
        /*
            Read the first part of the response. We don't know the content length and the server
            could be slow writing. Use an initial long delay. The server will respond with data or
            close the connection. Then shorten the delay as the server may use keep-alive and keep
            the connection open for the next request.
         */
        rc = rReadSocket(sock, response, sizeof(response) - 1, rGetTicks());
        if (rc < 0) {
            rFreeSocket(sock);
            return -1;
        }
        response[rc] = '\0';

        /*
            Basic validation: check if response looks like HTTP
         */
        if (rc >= 5 && memcmp(response, "HTTP/", 5) != 0) {
            rFreeSocket(sock);
            return -1;
        }
    }
    rFreeSocket(sock);
    return 1;
}

static bool testTLSRequest(cchar *fuzzInput, size_t len)
{
    int result;

    if (serverCrashed) {
        return 1;
    }
    currentFuzzInput = fuzzInput;
    currentFuzzLen = len;

    result = testTLSRequestInner(fuzzInput, len);

    if (result == -1) {
        return checkServerAlive("during TLS fuzzing");
    } else if (result == 0) {
        return 0;
    }
    return checkServerAlive("after processing TLS request");
}

/*
    TLS-specific mutation strategies
    Focus on:
    - HTTP requests that go over TLS
    - Protocol version variations
    - Cipher-sensitive patterns
    - Certificate validation edge cases
    - SNI and hostname handling
 */
static char *mutateTLSRequest(cchar *input, size_t *len)
{
    int strategy = random() % 50;

    switch (strategy) {
    // Generic mutations (10%)
    case 0: return fuzzBitFlip(input, len);
    case 1: return fuzzByteFlip(input, len);
    case 2: return fuzzInsertRandom(input, len);
    case 3: return fuzzDeleteRandom(input, len);
    case 4: return fuzzOverwriteRandom(input, len);

    // HTTP method mutations for HTTPS (8%)
    case 5: return fuzzReplace(input, len, "GET", "CONNECT");
    case 6: return fuzzReplace(input, len, "POST", "XPOST");
    case 7: return fuzzReplace(input, len, "GET", "G\x00T");
    case 8: return fuzzReplace(input, len, "GET", "get");

    // HTTP version mutations (6%)
    case 9: return fuzzReplace(input, len, "HTTP/1.1", "HTTP/2.0");
    case 10: return fuzzReplace(input, len, "HTTP/1.1", "HTTP/3.0");
    case 11: return fuzzReplace(input, len, "HTTP/1.0", "HTTP/0.9");

    // Host header mutations (critical for SNI) (10%)
    case 12: return fuzzReplace(input, len, "Host: localhost", "Host: ");
    case 13: return fuzzReplace(input, len, "Host: localhost", "Host: evil.com");
    case 14: return fuzzReplace(input, len, "Host: localhost", "Host: localhost\x00.evil.com");
    case 15: return fuzzReplace(input, len, "Host: localhost", "Host: localhost:99999");
    case 16: return fuzzReplace(input, len, "localhost", "local\nhost");

    // TLS-specific header injections (8%)
    case 17: {
        char   *addition = "\r\nUpgrade: TLS/1.0\r\nConnection: Upgrade";
        size_t alen = slen(addition);
        size_t newlen = *len + alen + 1;
        char   *result = rAlloc(newlen);
        memcpy(result, input, *len);
        memcpy(result + *len, addition, alen);
        *len = *len + alen;
        return result;
    }
    case 18: return fuzzReplace(input, len, "\r\n\r\n", "\r\nStrict-Transport-Security: max-age=0\r\n\r\n");
    case 19: return fuzzReplace(input, len, "\r\n\r\n", "\r\nExpect-CT: max-age=0\r\n\r\n");
    case 20: return fuzzReplace(input, len, "\r\n\r\n", "\r\nUpgrade-Insecure-Requests: 0\r\n\r\n");

    // Line ending mutations (8%)
    case 21: return fuzzReplace(input, len, "\r\n", "\n");
    case 22: return fuzzReplace(input, len, "\r\n", "\r");
    case 23: return fuzzReplace(input, len, "\r\n", "\r\n\r\n");
    case 24: return fuzzReplace(input, len, "\r\n\r\n", "\r\n");

    // Content-Length for encrypted payload (6%)
    case 25: return fuzzReplace(input, len, "Content-Length: 0", "Content-Length: -1");
    case 26: return fuzzReplace(input, len, "Content-Length: 0", "Content-Length: 999999");
    case 27: return fuzzReplace(input, len, "Content-Length: ", "Content-Length: 0");

    // Path mutations over TLS (8%)
    case 28: return fuzzReplace(input, len, "/", "//");
    case 29: return fuzzReplace(input, len, "/test/", "/test/../test/");
    case 30: return fuzzReplace(input, len, " HTTP", "%20HTTP");
    case 31: return fuzzReplace(input, len, "?", "%3F");

    // Special character injection (6%)
    case 32: return fuzzInsertSpecial(input, len);
    case 33: return fuzzReplace(input, len, "localhost", "local\x00host");
    case 34: return fuzzReplace(input, len, "index.html", "index\x00.html");

    // Structural mutations (6%)
    case 35: return fuzzDuplicate(input, len);
    case 36: return fuzzTruncate(input, len);
    case 37: return fuzzSplice(input, len);

    // Header name mutations (6%)
    case 38: return fuzzReplace(input, len, "Host", "X-Host");
    case 39: return fuzzReplace(input, len, "Content-Type", "Content-Type\x00");
    case 40: return fuzzReplace(input, len, "Accept", "Accept\r\nAccept");

    // Cookie mutations (4%)
    case 41: return fuzzReplace(input, len, "session=", "session=\x00");
    case 42: return fuzzReplace(input, len, "; ", ";");

    // Request smuggling patterns (6%)
    case 43: return fuzzReplace(input, len, "\r\n\r\n", "\r\n\r\nGET /smuggled HTTP/1.1\r\n\r\n");
    case 44: return fuzzReplace(input, len, "Transfer-Encoding: chunked",
                                "Transfer-Encoding: chunked\r\nTransfer-Encoding: identity");
    case 45: return fuzzReplace(input, len, "\r\n\r\n", "\r\n \r\n");

    // Large data (4%)
    case 46: {
        size_t newlen = *len + 1000;
        char   *result = rAlloc(newlen);
        memcpy(result, input, *len);
        memset(result + *len, 'A', 1000);
        *len = newlen;
        return result;
    }

    // Null byte injection (4%)
    case 47: {
        size_t newlen = *len + 1;
        char   *result = rAlloc(newlen);
        memcpy(result, input, *len);
        result[*len] = '\0';
        *len = newlen;
        return result;
    }

    // Certificate-related hostname mutations (4%)
    case 48: return fuzzReplace(input, len, "localhost", "*.localhost");
    case 49: return fuzzReplace(input, len, "localhost", "localhost.");

    default: return sclone(input);
    }
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
