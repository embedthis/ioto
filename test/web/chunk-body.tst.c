/*
    chunk-body.tst.c - Chunked request bodies on the wire

    The server strips chunk framing in place as the body arrives, so every reader of the request
    body sees plain data: the body reader, the line reader and the multipart upload parser. These
    tests drive the framing itself over raw sockets: bodies split across reads at every awkward
    point, trailer fields, a pipelined request behind the body, framing errors, and multipart
    uploads whose boundaries straddle the chunks.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

#define CHUNKED_POST "POST /test/show HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n"

#define BOUNDARY     "----WebTestBoundary"
#define UPLOAD_POST  "POST /test/show HTTP/1.1\r\nHost: x\r\n" \
                     "Content-Type: multipart/form-data; boundary=" BOUNDARY "\r\n"
#define PART_HEADERS "--" BOUNDARY "\r\n" \
                     "Content-Disposition: form-data; name=\"file\"; filename=\"data.txt\"\r\n" \
                     "Content-Type: text/plain\r\n\r\n"
#define PART_DATA    "The quick brown fox jumps over the lazy dog"
#define FINAL        "\r\n--" BOUNDARY "--\r\n"

/************************************ Code ************************************/

/*
    Send the segments, each in its own read at the server, and return the status
 */
static int split(cchar **segments, int count, char *buf, size_t size)
{
    return rawRequest(HTTP, segments, count, 20, 1, buf, size);
}

/*
    Every framing element split across reads: the size line from its CRLF, the data from its
    terminating CRLF, the CRLF itself, and the zero chunk from the final empty line.
 */
static void testBodySplitAcrossReads(void)
{
    cchar *segments[] = {
        CHUNKED_POST, "5", "\r\nHEL", "LO", "\r", "\n3\r\nabc\r\n", "0\r\n", "\r\n"
    };
    char buf[4096];
    int  status;

    status = split(segments, 8, buf, sizeof(buf));
    teqi(status, 200);
    tcontains(buf, "\"body\":\"HELLOabc\"");
}

/*
    A chunk-size line with leading zeros split at every byte must still be read as one line.
 */
static void testSizeLineSplitAcrossReads(void)
{
    cchar *segments[] = { CHUNKED_POST, "0", "0", "0", "5", "\r", "\n", "HELLO\r\n0\r\n\r\n" };
    char  buf[4096];
    int   status;

    status = split(segments, 8, buf, sizeof(buf));
    teqi(status, 200);
    tcontains(buf, "\"body\":\"HELLO\"");
}

/*
    RFC 9112 7.1.2: trailer fields may follow the last chunk. They are permitted and ignored.
 */
static void testTrailerFields(void)
{
    cchar *segments[] = {
        CHUNKED_POST "5\r\nHELLO\r\n0\r\nX-Checksum: abc123\r\nX-Other: 1\r\n\r\n"
    };
    char buf[4096];
    int  status;

    status = split(segments, 1, buf, sizeof(buf));
    teqi(status, 200);
    tcontains(buf, "\"body\":\"HELLO\"");
    ttrue(scontains(buf, "X-Checksum") == 0, "Trailer fields must not be echoed as headers");
}

/*
    A body of one zero chunk is an empty body, not an error
 */
static void testEmptyBody(void)
{
    cchar *segments[] = { CHUNKED_POST "0\r\n\r\n" };
    char  buf[4096];
    int   status;

    status = split(segments, 1, buf, sizeof(buf));
    teqi(status, 200);
    ttrue(scontains(buf, "\"body\"") == 0, "An empty chunked body must yield no body");
}

/*
    A request pipelined behind a chunked body arrives in the same read as the body. It must be
    served as a second request, not swallowed as body data or rejected as a bad chunk.
 */
static void testPipelinedRequestAfterBody(void)
{
    cchar *segments[] = {
        CHUNKED_POST "5\r\nHELLO\r\n0\r\n\r\nGET /index.html HTTP/1.1\r\nHost: x\r\n\r\n"
    };
    char  buf[16384];
    cchar *second;
    int   status;

    status = rawRequest(HTTP, segments, 1, 0, 2, buf, sizeof(buf));
    teqi(status, 200);
    tcontains(buf, "\"body\":\"HELLO\"");
    teqi(rawCountResponses(buf, (ssize) slen(buf)), 2);
    second = scontains(scontains(buf, "\r\n0\r\n\r\n") + 7, "HTTP/1.1 ");
    tnotnull(second);
    if (second) {
        teqi((int) stoi(&second[9]), 200);
    }
}

/*
    Framing errors are refused with 400, never accepted and never left to stall the connection
 */
static void testFramingErrorsRefused(void)
{
    cchar *badEnd[] = { CHUNKED_POST "5\r\nHELLOXX\r\n0\r\n\r\n" };
    cchar *shortData[] = { CHUNKED_POST "5\r\nHEL\r\n0\r\n\r\n" };
    cchar *tooBig[] = { CHUNKED_POST "40000\r\nHELLO\r\n0\r\n\r\n" };
    cchar *badTrailer[] = { CHUNKED_POST "5\r\nHELLO\r\n0\r\n", "no-crlf-here-", "-still-none" };
    char  buf[4096];

    teqi(split(badEnd, 1, buf, sizeof(buf)), 400, "Data past the chunk size must be refused");
    teqi(split(shortData, 1, buf, sizeof(buf)), 400, "A chunk shorter than its size must be refused");
    teqi(split(tooBig, 1, buf, sizeof(buf)), 400, "A chunk larger than the body limit must be refused");
    ttrue(split(badTrailer, 3, buf, sizeof(buf)) != 200, "An unterminated trailer must not succeed");
}

/*
    Chunk the multipart body so that the chunk edges fall inside the part boundary, inside the
    part headers and inside the closing boundary. The upload parser must see the parts intact.
 */
static void testUploadStraddlingChunks(void)
{
    cchar *segments[32];
    char  *pieces[8] = { 0 }, sizeLine[8][16], buf[4096];
    cchar *body;
    ssize len, end, size;
    /*
        Inside the opening boundary, at its end, inside the part headers, at the header/data edge,
        inside the data, and twice inside the closing boundary (188 bytes in all)
     */
    ssize cut[] = { 3, 23, 60, 118, 130, 163, 175 };
    int   i, n, count, status;

    body = PART_HEADERS PART_DATA FINAL;
    len = (ssize) slen(body);

    //  Cut the body into pieces at the chosen offsets and frame each piece as a chunk
    n = 0;
    count = 0;
    segments[count++] = UPLOAD_POST "Transfer-Encoding: chunked\r\n\r\n";
    for (i = 0; i <= 7; i++) {
        end = (i < 7) ? cut[i] : len;
        size = end - n;
        pieces[i] = snclone(&body[n], (size_t) size);
        SFMT(sizeLine[i], "%zx\r\n", (size_t) size);
        segments[count++] = sizeLine[i];
        segments[count++] = pieces[i];
        segments[count++] = "\r\n";
        n = (int) end;
    }
    segments[count++] = "0\r\n\r\n";

    status = split(segments, count, buf, sizeof(buf));
    teqi(status, 200);
    tcontains(buf, "\"clientFilename\":\"data.txt\"");
    tcontains(buf, "\"size\":43");

    for (i = 0; i <= 7; i++) {
        rFree(pieces[i]);
    }
}

/*
    Same upload with a Content-Length: the parser must work over both framings
 */
static void testUploadWithContentLength(void)
{
    cchar *segments[3];
    char  headers[512], buf[4096];
    int   status;

    SFMT(headers, UPLOAD_POST "Content-Length: %d\r\n\r\n", (int) slen(PART_HEADERS PART_DATA FINAL));
    segments[0] = headers;
    segments[1] = PART_HEADERS PART_DATA;
    segments[2] = FINAL;
    status = split(segments, 3, buf, sizeof(buf));
    teqi(status, 200);
    tcontains(buf, "\"size\":43");
}


static void testUploadUnframed(void)
{
    cchar *segments[] = { UPLOAD_POST "\r\n" };
    char  buf[4096];
    int   status;

    status = split(segments, 1, buf, sizeof(buf));
    teqi(status, 411);
}

/*
    A body that ends before the closing boundary must be refused promptly, not left waiting on
    the socket for data that will never come. With a Content-Length the end is the declared
    length; with chunking it is the zero chunk.
 */
static void testUploadBodyEndsBeforeFinalBoundary(void)
{
    cchar *chunked[] = { UPLOAD_POST "Transfer-Encoding: chunked\r\n\r\n", "2b\r\n" PART_DATA "\r\n0\r\n\r\n" };
    cchar *known[2];
    char  headers[512], buf[4096];
    Ticks started;
    int   status;

    SFMT(headers, UPLOAD_POST "Content-Length: %d\r\n\r\n", (int) slen(PART_HEADERS PART_DATA));
    known[0] = headers;
    known[1] = PART_HEADERS PART_DATA;

    started = rGetTicks();
    status = split(known, 2, buf, sizeof(buf));
    teqi(status, 400, "A Content-Length body ending before the final boundary must be refused");
    ttrue(rGetTicks() - started < 2 * TPS, "The refusal must not wait for a socket timeout");

    started = rGetTicks();
    status = split(chunked, 2, buf, sizeof(buf));
    teqi(status, 400, "A chunked body ending before the first boundary must be refused");
    ttrue(rGetTicks() - started < 2 * TPS, "The refusal must not wait for a socket timeout");
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        testBodySplitAcrossReads();
        testSizeLineSplitAcrossReads();
        testTrailerFields();
        testEmptyBody();
        testPipelinedRequestAfterBody();
        testFramingErrorsRefused();
        testUploadStraddlingChunks();
        testUploadWithContentLength();
        testUploadUnframed();
        testUploadBodyEndsBeforeFinalBoundary();
    }
    rFree(HTTP);
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
