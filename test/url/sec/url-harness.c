/*
    url-harness.c - HTTP response parser fuzz tests. See README.md for usage. Include urlLib.c to access static parsers. An expired deadline prevents socket reads.

    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
 */

/********************************** Includes **********************************/

#include "harness.h"

#ifndef SEC_LIBFUZZER
#include "fuzzcore/fuzz.c"
#endif

/*
    Include the source to access static parsers.
 */
#define ME_COM_URL 1
#include "urlLib.c"

/*********************************** Locals ***********************************/

/*
    Reuse one Url, resetting parser state between inputs.
 */
static Url *gUrl;

static void initRuntime(void)
{
    static int initialised = 0;

    if (!initialised) {
        initialised = 1;
        rInit(0, 0);
        gUrl = urlAlloc(0);
        gUrl->method = sclone("GET");
        /*
            An expired deadline prevents socket reads after buffered input runs out.
         */
        gUrl->deadline = 0;
    }
}

/*
    Bound diagnostic output because fuzz input is not NUL-terminated.
 */
static int clampLen(size_t len)
{
    return (int) (len < 80 ? len : 80);
}

/*
    Search binary input without stopping at embedded NULs.
 */
static cchar *memFind(cchar *hay, size_t hayLen, cchar *needle, size_t needleLen)
{
    size_t i;

    if (needleLen == 0) {
        return hay;
    }
    if (needleLen > hayLen) {
        return NULL;
    }
    for (i = 0; i + needleLen <= hayLen; i++) {
        if (memcmp(hay + i, needle, needleLen) == 0) {
            return hay + i;
        }
    }
    return NULL;
}

/********************************** Oracles ***********************************/

/*
    Check status and body bounds. Match readUntil's NUL-terminated header search.
 */
static bool responseOracle(cchar *input, size_t len)
{
    Url   *up;
    cchar *terminator;
    ssize headerSize, avail, total;
    RBuf  *body;
    bool  ok;

    initRuntime();
    up = gUrl;
    resetState(up);
    ok = true;

    rPutBlockToBuf(up->rx, input, len);
    rAddNullToBuf(up->rx);

    terminator = strstr(rGetBufStart(up->rx), "\r\n\r\n");
    if (!terminator) {
        return true;
    }
    headerSize = (ssize) ((terminator - rGetBufStart(up->rx)) + 4);

    if (parseResponse(up, (size_t) headerSize) < 0) {
        return true;
    }
    if (up->status < 100 || up->status > 599) {
        fprintf(stderr, "sec: url response: status %d outside [100,599] (input \"%.*s\")\n",
                up->status, clampLen(len), input);
        ok = false;
    }

    avail = (ssize) len - headerSize;
    up->finalized = 1;
    up->wroteHeaders = 1;

    body = urlGetResponseBuf(up);
    if (body) {
        total = (ssize) rGetBufLength(body);
        if (total > avail) {
            fprintf(stderr,
                    "sec: url response: urlGetResponseBuf returned %zd bytes but only %zd were sent "
                    "(input \"%.*s\")\n", total, avail, clampLen(len), input);
            ok = false;
        }
    }
    return ok;
}

/*
    Match readUntil's strstr search, including its stop at embedded NULs.
 */
static cchar *crlfFind(cchar *p, size_t n)
{
    size_t strLen;

    strLen = 0;
    while (strLen < n && p[strLen] != '\0') {
        strLen++;
    }
    return memFind(p, strLen, "\r\n", 2);
}

/*
    Compare chunk decoding with an independent RFC 7230 reference decoder.
 */
static ssize referenceChunkSum(cchar *input, size_t len, bool *wellFormed)
{
    size_t pos, i, j, lineLen;
    uint64 size;
    int    anyDigit, v;
    ssize  sum;
    cchar  *lineEnd;

    *wellFormed = false;
    pos = 0;
    sum = 0;

    for (;;) {
        lineEnd = crlfFind(input + pos, len - pos);
        if (!lineEnd) {
            return sum;
        }
        lineLen = (size_t) (lineEnd - (input + pos));
        if (lineLen == 0) {
            return sum;
        }
        size = 0;
        anyDigit = 0;
        for (i = 0; i < lineLen; i++) {
            char c = input[pos + i];
            if (c >= '0' && c <= '9') {
                v = c - '0';
            } else if (c >= 'a' && c <= 'f') {
                v = c - 'a' + 10;
            } else if (c >= 'A' && c <= 'F') {
                v = c - 'A' + 10;
            } else {
                break;
            }
            if (size > (UINT64_MAX >> 4)) {
                return sum;
            }
            size = size * 16 + (uint64) v;
            anyDigit = 1;
        }
        if (!anyDigit) {
            return sum;
        }
        j = i;
        while (j < lineLen && (input[pos + j] == ' ' || input[pos + j] == '\t')) {
            j++;
        }
        if (j < lineLen && input[pos + j] != ';') {
            return sum;
        }
        pos += lineLen + 2;

        if (size == 0) {
            for (;;) {
                lineEnd = crlfFind(input + pos, len - pos);
                if (!lineEnd) {
                    return sum;
                }
                lineLen = (size_t) (lineEnd - (input + pos));
                pos += lineLen + 2;
                if (lineLen == 0) {
                    break;
                }
            }
            *wellFormed = true;
            return sum;
        }
        if (size > (uint64) (len - pos) || pos + size + 2 > len) {
            return sum;
        }
        if (input[pos + size] != '\r' || input[pos + size + 1] != '\n') {
            return sum;
        }
        sum += (ssize) size;
        pos += size + 2;
    }
}

static bool chunkOracle(cchar *input, size_t len)
{
    Url   *up;
    char  tmp[512];
    ssize n, total, refSum;
    int   iterations;
    bool  wellFormed, ok;

    initRuntime();
    up = gUrl;
    resetState(up);
    ok = true;

    up->chunked = URL_CHUNK_START;
    up->rxLen = -1;

    rPutBlockToBuf(up->rx, input, len);
    rAddNullToBuf(up->rx);

    refSum = referenceChunkSum(input, len, &wellFormed);

    total = 0;
    iterations = 0;
    while (up->chunked != URL_CHUNK_EOF && iterations++ < 10000) {
        n = readChunk(up, tmp, sizeof(tmp));
        if (n <= 0) {
            break;
        }
        total += n;
    }
    if ((size_t) total > len) {
        fprintf(stderr, "sec: url chunk: decoded %zd bytes but only %zu were supplied (input \"%.*s\")\n",
                total, len, clampLen(len), input);
        ok = false;
    }
    if (wellFormed) {
        if (up->chunked != URL_CHUNK_EOF) {
            fprintf(stderr,
                    "sec: url chunk: reference decoder calls this well-formed (sum %zd) but readChunk "
                    "never reached EOF (input \"%.*s\")\n", refSum, clampLen(len), input);
            ok = false;
        } else if (total != refSum) {
            fprintf(stderr,
                    "sec: url chunk: decoded length %zd disagrees with the declared chunk-size sum %zd "
                    "(input \"%.*s\")\n", total, refSum, clampLen(len), input);
            ok = false;
        }
    }
    return ok;
}

/*
    Check each emitted data line against input, excluding inserted newline separators.
 */
static cchar *gSseInput;
static size_t gSseInputLen;
static bool   gSseOk;

static void sseCallback(Url *up, ssize id, cchar *event, cchar *data, void *arg)
{
    char *copy, *line, *last;

    (void) up;
    (void) id;
    (void) event;
    (void) arg;

    if (!data) {
        return;
    }
    copy = sclone(data);
    line = stok(copy, "\n", &last);
    while (line) {
        if (*line && !memFind(gSseInput, gSseInputLen, line, slen(line))) {
            fprintf(stderr, "sec: url sse: emitted data segment \"%.60s\" is not a substring of the input\n",
                    line);
            gSseOk = false;
        }
        line = stok(NULL, "\n", &last);
    }
    rFree(copy);
}

static bool sseOracle(cchar *input, size_t len)
{
    Url *up;

    initRuntime();
    up = gUrl;
    resetState(up);

    up->sse = 1;
    up->sseProc = sseCallback;
    up->sseArg = NULL;

    if (!up->responseBuf) {
        up->responseBuf = rAllocBuf(0);
    }
    rPutBlockToBuf(up->responseBuf, input, len);
    rAddNullToBuf(up->responseBuf);

    gSseInput = input;
    gSseInputLen = len;
    gSseOk = true;

    parseEvents(up);

    return gSseOk;
}

/*
    Check lookup bounds even after parse failure, which may leave partially tokenized headers.
 */
static bool headersOracle(cchar *input, size_t len)
{
    static cchar *names[] = {
        "content-length", "transfer-encoding", "connection", "location",
        "last-event-id", "set-cookie", "date", "x-fuzz-absent-header",
    };
    Url   *up;
    RBuf  *hdrBuf;
    cchar *bufStart, *bufEnd, *value, *nul;
    char  *cookie;
    size_t i;
    bool  ok;

    initRuntime();
    up = gUrl;
    resetState(up);
    ok = true;
    up->status = URL_CODE_OK;

    hdrBuf = rAllocBuf(len + 1);
    rPutBlockToBuf(hdrBuf, input, len);
    up->rxHeaders = hdrBuf;

    // Check lookups after both successful and failed parses.
    parseHeaders(up);

    bufStart = rGetBufStart(up->rxHeaders);
    bufEnd = up->rxHeaders->end;

    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        value = urlGetHeader(up, names[i]);
        if (!value) {
            continue;
        }
        if (value < bufStart || value > bufEnd) {
            fprintf(stderr,
                    "sec: url headers: urlGetHeader(\"%s\") returned a pointer outside the header "
                    "buffer (input \"%.*s\")\n", names[i], clampLen(len), input);
            ok = false;
            continue;
        }
        //  RBuf keeps a NUL at bufEnd, which terminates a value that runs to the end of the buffer
        nul = memchr(value, '\0', (size_t) (bufEnd - value) + 1);
        if (!nul) {
            fprintf(stderr,
                    "sec: url headers: urlGetHeader(\"%s\") value is not NUL-terminated within the "
                    "header buffer (input \"%.*s\")\n", names[i], clampLen(len), input);
            ok = false;
        }
    }
    cookie = urlGetCookie(up, "sid");
    if (cookie) {
        rFree(cookie);
    }
    return ok;
}

#if defined(URL_ORACLE_CHUNK)
    #define ORACLE      chunkOracle
    #define ORACLE_NAME "chunk"
#elif defined(URL_ORACLE_SSE)
    #define ORACLE      sseOracle
    #define ORACLE_NAME "sse"
#elif defined(URL_ORACLE_HEADERS)
    #define ORACLE      headersOracle
    #define ORACLE_NAME "headers"
#else
    #define ORACLE      responseOracle
    #define ORACLE_NAME "response"
#endif

/****************************** libFuzzer entry *******************************/

#ifdef SEC_LIBFUZZER
SEC_LIBFUZZER_ENTRY(ORACLE)

#else

/*********************************** main *************************************/

int main(int argc, char **argv)
{
    FuzzConfig config;
    FuzzRunner *runner;
    cchar      *replay;
    int        crashes, leaked;

    (void) argc;
    (void) argv;

    initRuntime();

    memset(&config, 0, sizeof(config));
    config.stop = false;
    config.verbose = false;
    /*
        fuzzInit creates only one directory level.
     */
    config.crashDir = "crashes";

    runner = fuzzInit(&config);
    fuzzSetOracle(runner, ORACLE);

#if defined(URL_ORACLE_CHUNK)
    {
        static cchar *seeds[] = {
            "5\r\nhello\r\n0\r\n\r\n",
            "0\r\n\r\n",
            "3\r\nabc\r\n5\r\nworld\r\n0\r\n\r\n",
            "3;ext=1\r\nabc\r\n0\r\n\r\n",
            "0\r\nX-Trailer: value\r\n\r\n",
            "A\r\n0123456789\r\n0\r\n\r\n",
            "5 \r\nhello\r\n0\r\n\r\n",
            "5;\r\nhello\r\n0\r\n\r\n",
            "-1\r\nbad\r\n",
            "zz\r\n\r\n",
            "5\r\nhel",
            "",
        };
        size_t s;
        for (s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++) {
            fuzzAddCorpus(runner, seeds[s], strlen(seeds[s]));
        }
    }
#elif defined(URL_ORACLE_SSE)
    {
        static cchar *seeds[] = {
            "data: hello\n\n",
            "event: msg\ndata: hi\n\n",
            "id: 5\nevent: update\ndata: line1\ndata: line2\n\n",
            ": this is a comment\n\ndata: after comment\n\n",
            "data: \n\n",
            "data:no-space\n\n",
            "unknownfield: x\ndata: still works\n\n",
            "\n\n",
            "data: multi\ndata: part\ndata: event\n\ndata: second\n\n",
            "",
        };
        size_t s;
        for (s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++) {
            fuzzAddCorpus(runner, seeds[s], strlen(seeds[s]));
        }
    }
#elif defined(URL_ORACLE_HEADERS)
    {
        static cchar *seeds[] = {
            "Content-Length: 42\r\n",
            "Transfer-Encoding: chunked\r\n",
            "Connection: close\r\n",
            "Location: /redirect\r\n",
            "Set-Cookie: sid=abc123; Path=/\r\n",
            "X-Custom: value with spaces \r\n",
            "Content-Length: 5\r\nContent-Length: 5\r\n",
            "Bad Header Without Colon\r\n",
            "Content-Length: -1\r\n",
            "\r\n",
            "",
        };
        size_t s;
        for (s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++) {
            fuzzAddCorpus(runner, seeds[s], strlen(seeds[s]));
        }
    }
#else
    {
        static cchar *seeds[] = {
            "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHello",
            "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n",
            "HTTP/1.1 204 No Content\r\n\r\nignored-body-bytes",
            "HTTP/1.0 200 OK\r\n\r\nclose-delimited body until EOF",
            "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\n\r\n",
            "HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nhi",
            "HTTP/1.1 999 Bad\r\n\r\n",
            "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nContent-Length: 6\r\n\r\nhello!",
            "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nrest-of-stream",
            "GARBAGE NOT HTTP\r\n\r\n",
            "HTTP/1.1 200 OK\r\n\r\n",
            "HTTP/1.1 20 OK\r\n\r\n",
            "",
        };
        size_t s;
        for (s = 0; s < sizeof(seeds) / sizeof(seeds[0]); s++) {
            fuzzAddCorpus(runner, seeds[s], strlen(seeds[s]));
        }
    }
#endif
    // fuzzLoadDir does not recurse.
    fuzzLoadDir(runner, "corpus/" ORACLE_NAME);

    replay = getenv("FUZZ_REPLAY");
    if (replay) {
        printf("Replay mode: %s\n", replay);
        fuzzClearCorpus(runner);
        fuzzLoadArtifact(runner, replay);
    }
    printf("Oracle: %s\n", ORACLE_NAME);

    crashes = fuzzRun(runner);
    fuzzReport(runner);
    fuzzFree(runner);

    leaked = fuzzLeakCheck();

    return (crashes > 0 || leaked > 0) ? 1 : 0;
}

#endif /* SEC_LIBFUZZER */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
 */
