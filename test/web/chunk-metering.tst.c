/*
    chunk-metering.tst.c - Chunk framing and trailer bytes are metered against the body limit

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define ENDPOINT "http://127.0.0.1:4859"

/************************************ Code ************************************/

static int post(cchar *body)
{
    cchar *segments[1];
    char  *req, buf[8192];
    int   status;

    req = sfmt("POST /test/show HTTP/1.1\r\nHost: 127.0.0.1:4859\r\nConnection: close\r\n"
               "Content-Type: text/plain\r\nTransfer-Encoding: chunked\r\n\r\n%s", body);
    segments[0] = req;
    status = rawRequest(ENDPOINT, segments, 1, 0, 1, buf, sizeof(buf));
    rFree(req);
    return status;
}

static void testFramingMetered(void)
{
    WebHost *host;
    RBuf    *body;
    char    ext[101], data[256];
    int     i, status;

    host = startTestHost("{ web: { documents: './site/', listen: ['" ENDPOINT "'], limits: { body: '1K' }, "
                         "routes: [ { match: '/test/', handler: 'action' }, {} ] } }");
    if (!host) {
        return;
    }
    memset(ext, 'x', sizeof(ext) - 1);
    ext[sizeof(ext) - 1] = '\0';
    memset(data, 'a', sizeof(data));

    //  20 bytes of data carried by about 2K of chunk extensions
    body = rAllocBuf(4096);
    for (i = 0; i < 20; i++) {
        rPutToBuf(body, "1;%s\r\na\r\n", ext);
    }
    rPutStringToBuf(body, "0\r\n\r\n");
    status = post(rBufToString(body));
    teqi(status, 413, "Chunk framing past the body limit is refused");
    rFreeBuf(body);

    //  A long trailer section
    body = rAllocBuf(4096);
    rPutStringToBuf(body, "1\r\na\r\n0\r\n");
    for (i = 0; i < 20; i++) {
        rPutToBuf(body, "X-Trailer-%d: %s\r\n", i, ext);
    }
    rPutStringToBuf(body, "\r\n");
    status = post(rBufToString(body));
    teqi(status, 413, "Trailer bytes past the body limit are refused");
    rFreeBuf(body);

    //  A body at the limit in ordinary chunks is accepted
    body = rAllocBuf(4096);
    for (i = 0; i < 4; i++) {
        rPutToBuf(body, "100\r\n");
        rPutBlockToBuf(body, data, sizeof(data));
        rPutStringToBuf(body, "\r\n");
    }
    rPutStringToBuf(body, "0\r\n\r\n");
    status = post(rBufToString(body));
    teqi(status, 200, "A body at the limit in ordinary chunks is accepted");
    rFreeBuf(body);

    stopTestHost(host);
}

static void fiberMain(void *arg)
{
    if (setup(NULL, NULL)) {
        testFramingMetered();
    }
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
