/*
    head-range.tst.c - A HEAD response ends at the header terminator

    A multi-range request is framed chunked (txLen = -1). On HEAD the file handler skips the body
    but still finalizes, and the finalizing write carries no data, so it slipped past the body
    suppression in webWrite and emitted the terminating chunk - five bytes after the blank line
    that ends the response. A conforming proxy stops reading at that blank line and reads those
    bytes as the start of the next response, misaligning every later response on the connection.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

#define RANGES "bytes=0-9,20-29"

/************************************ Code ************************************/

/*
    Pipeline a HEAD carrying "range" with a following GET on one connection, and require the second
    response to begin immediately after the first response's header terminator.
 */
static void checkNothingAfterTheHeaders(cchar *range, cchar *why)
{
    cchar *segments[1];
    char  req[512], buf[16384], *sep;
    int   status;

    SFMT(req, "HEAD /range-test.txt HTTP/1.1\r\nHost: x\r\n%s%s%s\r\n"
         "GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n",
         range ? "Range: " : "", range ? range : "", range ? "\r\n" : "");
    segments[0] = req;
    status = rawRequest(HTTP, segments, 1, 0, 2, buf, sizeof(buf));
    teqi(status, range ? 206 : 200, "%s: unexpected status", why);

    if ((sep = scontains(buf, "\r\n\r\n")) == 0) {
        tfail("%s: no header terminator in the response", why);
        return;
    }
    ttrue(sstarts(&sep[4], "HTTP/1.1"),
          "%s: the next response must start at the header terminator, found '%.8s'", why, &sep[4]);
}

static void testHeadEmitsNoBody(void)
{
    checkNothingAfterTheHeaders(RANGES, "A multi-range HEAD");
    checkNothingAfterTheHeaders("bytes=0-9", "A single-range HEAD");
    checkNothingAfterTheHeaders(NULL, "A plain HEAD");
}

/*
    The equivalent GET must still produce a correct multipart/byteranges 206.
 */
static void testGetRangeUnchanged(void)
{
    Url   *up;
    cchar *contentType, *response;
    char  url[256];
    int   status;

    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0, "Range: " RANGES "\r\n");
    teqi(status, 206, "A multi-range GET must still be a 206");
    contentType = urlGetHeader(up, "Content-Type");
    ttrue(contentType && sstarts(contentType, "multipart/byteranges"),
          "A multi-range GET must still be multipart/byteranges");
    response = urlGetResponse(up);
    tcontains(response, "0123456789");
    tcontains(response, "0123456789\r\n--");
    urlFree(up);
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        testHeadEmitsNoBody();
        testGetRangeUnchanged();
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
