

/*
    buffer.c.tst - Tests for output buffering

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void testBuffer(void)
{
    Url   *up;
    cchar *contentLength, *response;
    char  url[128];
    int   status;

    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/test/buffer", HTTP), NULL, 0, NULL);
    ttrue(status == 200);
    response = urlGetResponse(up);

    /*
        Expect buffered output to have a content length and not be transfer encoded.
     */
    contentLength = urlGetHeader(up, "Content-Length");
    ttrue(stoi(contentLength) > 0);

    ttrue(scontains(response, "Hello World 1"));
    ttrue(scontains(response, "Hello World 7"));
    urlFree(up);
}


static void testBufferDoesNotOutliveResponse(void)
{
    cchar *segments[1];
    char  buf[16384];
    int   status;

    segments[0] = "GET /test/buffer HTTP/1.1\r\nHost: x\r\n\r\n"
                  "GET /index.html HTTP/1.1\r\nHost: x\r\nRange: bytes=0-1,4-5\r\nConnection: close\r\n\r\n";
    status = rawRequest(HTTP, segments, 1, 0, 2, buf, sizeof(buf));
    teqi(status, 200);
    tcontains(buf, "206 Partial Content");
    teqi(rawCountResponses(buf, (ssize) slen(buf)), 2, "A chunked response after a buffered one must be terminated");
}


static void testHeadAfterBuffer(void)
{
    cchar       *segments[1], *cp, *second;
    char        buf[16384];
    struct stat info;
    int         status;

    teqi(stat("site/index.html", &info), 0);
    segments[0] = "GET /test/buffer HTTP/1.1\r\nHost: x\r\n\r\n"
                  "HEAD /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n";
    status = rawRequest(HTTP, segments, 1, 0, 3, buf, sizeof(buf));
    teqi(status, 200);
    //  The buffered body ends in a newline, so find the second status line after the first
    second = scontains(&buf[1], "HTTP/1.1 ");
    tnotnull(second);
    if (second && (cp = sncaselesscontains(second, "Content-Length:", slen(second))) != 0) {
        teqll(stoi(&cp[15]), (int64) info.st_size, "HEAD must report the file length");
    } else {
        tfail("The HEAD response must carry a Content-Length");
    }
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testBuffer();
        testBufferDoesNotOutliveResponse();
        testHeadAfterBuffer();
    }
    rFree(HTTP);
    rFree(HTTPS);
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
