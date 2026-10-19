/*
    chunk-trailer.tst.c - Chunked framing lines and the trailer section are parsed strictly

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define ENDPOINT "http://127.0.0.1:4886"
#define HEAD     "POST /test/success HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n"
#define BODY     HEAD "5\r\nhello\r\n0\r\n"
#define NEXT     "GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n"

/************************************ Code ************************************/


static int sendRequest(cchar *request, int *count)
{
    cchar *segments[1];
    char  *req, buf[16384];
    int   status;

    req = sfmt("%s" NEXT, request);
    segments[0] = req;
    status = rawRequest(ENDPOINT, segments, 1, 0, 2, buf, sizeof(buf));
    rFree(req);
    *count = rawCountResponses(buf, (ssize) slen(buf));
    return status;
}

static void testTrailerSection(void)
{
    int count, status;

    status = sendRequest(BODY "\r\n", &count);
    teqi(status, 200, "No trailer: body accepted");
    teqi(count, 2, "No trailer: the pipelined request is served");

    status = sendRequest(BODY "X-Trailer: 1\r\n\r\n", &count);
    teqi(status, 200, "A field-line trailer is accepted");
    teqi(count, 2, "A field-line trailer: the pipelined request is served");

    status = sendRequest(BODY "\n", &count);
    teqi(status, 400, "A bare LF in the trailer section must be refused, not skipped");
    status = sendRequest(BODY "GET /x HTTP/1.1\r\n\r\n", &count);
    teqi(status, 400, "A trailer line that is not a field line must be refused");
    status = sendRequest(BODY "X-A: 1\rX-B: 2\r\n\r\n", &count);
    teqi(status, 400, "A bare CR in a trailer line must be refused");
}

static void testChunkSizeLine(void)
{
    int count, status;

    status = sendRequest(HEAD "5;ext=1\r\nhello\r\n0\r\n\r\n", &count);
    teqi(status, 200, "A chunk extension is accepted");
    status = sendRequest(HEAD "5\rX\r\nhello\r\n0\r\n\r\n", &count);
    teqi(status, 400, "A bare CR after the chunk size must be refused");
    status = sendRequest(HEAD "5\nhello\r\n0\r\n\r\n", &count);
    teqi(status, 400, "A bare LF after the chunk size must be refused");
    status = sendRequest(HEAD "5;a\nb\r\nhello\r\n0\r\n\r\n", &count);
    teqi(status, 400, "A bare LF in a chunk extension must be refused");
}

static void testHttp10TransferEncoding(void)
{
    int count, status;

    status = sendRequest("POST /test/success HTTP/1.0\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n"
                         "5\r\nhello\r\n0\r\n\r\n", &count);
    teqi(status, 400, "Transfer-Encoding on HTTP/1.0 must be refused");
}

static void fiberMain(void *arg)
{
    WebHost *host;

    host = startTestHost("{ web: { documents: './site/', listen: ['" ENDPOINT "'], "
                         "routes: [ { match: '/test/', handler: 'action', methods: ['POST'] }, {} ] } }");
    if (host) {
        testTrailerSection();
        testChunkSizeLine();
        testHttp10TransferEncoding();
        stopTestHost(host);
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
    This is proprietary software and requires a commercial license from the author.
 */
