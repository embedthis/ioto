/*
    chunk-framing.tst.c - Chunk-size line validation

    RFC 9112 7.1 defines chunk-size as 1*HEXDIG and bounds neither the digit count nor leading
    zeros, so a padded chunk-size line is legal and any conforming front-end forwards it intact.

    Two defects lived here. A line longer than the 32-byte read buffer was truncated, mis-parsed
    and the body silently dropped while the response was still 200 - so a proxy and this server
    disagreed about where the request ended, which is a request-smuggling primitive. Separately an
    out-of-range size such as FFFFFFFFFFFFFFFFFF was accepted rather than refused, and the
    connection then stalled waiting for a body that could never arrive.

    These run over raw sockets: the point is the framing on the wire, not what a client library
    would let us express.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

/************************************ Code ************************************/

/*
    Send one chunked POST with the given chunk-size line and return the response status.
    Sets *sawBody to whether the server reported receiving the 5-byte body.
 */
static int chunkProbe(cchar *sizeLine, bool *sawBody)
{
    cchar *segments[1];
    char  req[512], buf[4096];
    int   status;

    *sawBody = 0;
    SFMT(req, "POST /test/show HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n"
         "%s\r\nHELLO\r\n0\r\n\r\n", sizeLine);
    segments[0] = req;
    status = rawRequest(HTTP, segments, 1, 0, 1, buf, sizeof(buf));
    //  /test/show echoes a printable request body, so the body itself is the signal
    *sawBody = scontains(buf, "HELLO") != 0;
    return status;
}

/*
    Legal chunk-size forms must deliver the body. The padded lines are the regression: they used
    to return 200 with the body silently discarded.
 */
static void testLegalChunkSizesDeliverBody(void)
{
    bool sawBody;
    char line[128];
    int  status;

    status = chunkProbe("5", &sawBody);
    teqi(status, 200);
    ttrue(sawBody, "A plain chunk-size must deliver the body");

    status = chunkProbe("00000005", &sawBody);
    teqi(status, 200);
    ttrue(sawBody, "Leading zeros are legal and must deliver the body");

    //  31 zeros then '5' - 32 characters, the old buffer size exactly
    memset(line, '0', 31);
    line[31] = '5';
    line[32] = '\0';
    status = chunkProbe(line, &sawBody);
    teqi(status, 200);
    ttrue(sawBody, "A 32-character chunk-size line must not silently drop the body");

    //  40 zeros then '5'
    memset(line, '0', 40);
    line[40] = '5';
    line[41] = '\0';
    status = chunkProbe(line, &sawBody);
    teqi(status, 200);
    ttrue(sawBody, "A 41-character chunk-size line must not silently drop the body");

    //  A chunk extension is legal and must be ignored, not rejected
    status = chunkProbe("5;name=value", &sawBody);
    teqi(status, 200);
    ttrue(sawBody, "A chunk-ext must be accepted and the body delivered");
}

/*
    Malformed or out-of-range sizes must be refused with 400, never accepted and never left to
    stall the connection.
 */
static void testMalformedChunkSizesRefused(void)
{
    bool sawBody;
    char line[256];
    int  status;

    status = chunkProbe("FFFFFFFFFFFFFFFFFF", &sawBody);
    teqi(status, 400, "An out-of-range chunk size must be refused, not stall the connection");

    status = chunkProbe(" 5", &sawBody);
    teqi(status, 400, "Leading whitespace in a chunk-size must be refused");

    status = chunkProbe("-5", &sawBody);
    teqi(status, 400, "A signed chunk-size must be refused");

    status = chunkProbe("zz", &sawBody);
    teqi(status, 400, "A non-hex chunk-size must be refused");

    //  Longer than the read buffer - must be refused rather than truncated and mis-parsed
    memset(line, '0', 200);
    line[200] = '5';
    line[201] = '\0';
    status = chunkProbe(line, &sawBody);
    teqi(status, 400, "An over-long chunk-size line must be refused");
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        testLegalChunkSizesDeliverBody();
        testMalformedChunkSizesRefused();
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
