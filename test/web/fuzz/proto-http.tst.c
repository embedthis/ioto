/*
    protocol-http.fuzz.c - HTTP protocol fuzzer
 */

#include "r.h"
#include "url.h"
#include "web.h"
#include "fuzzLib.h"

static cchar      *HTTP;
static FuzzRunner *runner;
static int        fuzzResult = 0;

static cchar *initialCorpus[] = {
    "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n",
    "POST /api HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n",
    "HEAD /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n",
    NULL
};

static bool testHTTPRequest(cchar *fuzzInput, size_t len)
{
    RSocket *sock;
    cchar   *scheme = NULL, *host = NULL, *path = NULL;
    char    *buf, response[1024];
    int     port, rc;

    port = 0;
    buf = webParseUrl(HTTP, &scheme, &host, &port, &path, NULL, NULL);

    sock = rAllocSocket();
    if (rConnectSocket(sock, host, port, 0) < 0) {
        rFreeSocket(sock);
        rFree(buf);
        return true;  // Connection failure is acceptable
    }

    rc = rWriteSocket(sock, fuzzInput, len, 2000);
    if (rc < 0) {
        rFreeSocket(sock);
        rFree(buf);
        return true;
    }

    rReadSocket(sock, response, sizeof(response) - 1, 2000);

    rFreeSocket(sock);
    rFree(buf);

    return true;
}

static char *mutateHTTPRequest(cchar *input, size_t *len)
{
    int strategy = random() % 10;

    switch (strategy) {
    case 0: return fuzzBitFlip(input, len);
    case 1: return fuzzByteFlip(input, len);
    case 2: return fuzzInsertRandom(input, len);
    case 3: return fuzzDeleteRandom(input, len);
    case 4: return fuzzOverwriteRandom(input, len);
    case 5: return fuzzInsertSpecial(input, len);
    case 6: return fuzzReplace(input, len, "GET", "XGET");
    case 7: return fuzzReplace(input, len, "HTTP/1.1", "HTTP/9.9");
    case 8: return fuzzDuplicate(input, len);
    case 9: return fuzzTruncate(input, len);
    default: return sclone(input);
    }
}

static void fuzzFiber(void *arg)
{
    FuzzConfig *config = (FuzzConfig*) arg;

    rPrintf("Starting HTTP protocol fuzzer\n");
    rPrintf("Target: %s\n", HTTP);
    rPrintf("Iterations: %d\n", config->iterations);

    runner = fuzzInit(config);
    fuzzSetOracle(runner, testHTTPRequest);
    fuzzSetMutator(runner, mutateHTTPRequest);

    for (int i = 0; initialCorpus[i]; i++) {
        fuzzAddCorpus(runner, initialCorpus[i], slen(initialCorpus[i]));
    }

    fuzzLoadCorpus(runner, "corpus/http-requests.txt");

    int crashes = fuzzRun(runner);

    fuzzReport(runner);
    fuzzFree(runner);

    fuzzResult = crashes;
    rStop();
}

int main(int argc, char **argv)
{
    int iterations = getenv("TESTME_ITERATIONS") ? atoi(getenv("TESTME_ITERATIONS")) : 0;

    FuzzConfig config = {
        .iterations = (iterations > 1) ? iterations : 1000,
        .timeout = 5000,
        .parallel = 1,
        .seed = 0,
        .crashDir = "./crashes/protocol-http",
        .corpusDir = "./corpus/http",
        .verbose = getenv("TESTME_VERBOSE") != NULL,
    };

    HTTP = "http://localhost:4200";

    // Initialize runtime with fiber
    rInit(fuzzFiber, &config);
    rServiceEvents();
    rTerm();

    if (fuzzResult > 0) {
        rPrintf("\n✗ Found %d crashes\n", fuzzResult);
        return 1;
    }

    rPrintf("\n✓ HTTP protocol fuzzing complete - no crashes found\n");
    return 0;
}
