/*
    upload-boundary.tst.c - An over-long multipart boundary must be refused, not spun on

    processUploadData locates the end of a part with a scan bounded by a fixed compile-time limit,
    but the boundary token comes from the request. A boundary at or above that limit can never be
    matched inside it, so the scan returned "not yet" without reading, the data length came out zero,
    and the loop repeated on an unchanged buffer. Ioto is single-threaded and fiber-scheduled, so the
    whole server stopped answering every connection, unauthenticated.

    The second case uses an in-process host with a raised header limit, because the boundary has to
    exceed the desktop scan limit to reproduce the original spin. Port 4866 is used by this test
    alone.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

#define BIG_ENDPOINT "http://127.0.0.1:4866"

//  The header limit has to admit a boundary longer than the part scan limit
#define CONFIG_BIG                                                          \
    "{web: {documents: './site', listen: ['" BIG_ENDPOINT "'],"             \
    " limits: {header: '512K', body: '1MB'},"                               \
    " routes: [{match: '/test/', handler: 'action'}, {}]}}"

/************************************ Code ************************************/

/*
    POST a one-part multipart body whose boundary token is "len" characters long. Returns the
    status, or 0 if the server never answered.
 */
static int uploadWithBoundary(cchar *endpoint, size_t len, char *buf, size_t size)
{
    cchar *segments[1];
    char  *token, *body, *req;
    int   status;

    token = rAlloc(len + 1);
    memset(token, 'X', len);
    token[len] = '\0';

    body = sfmt("--%s\r\nContent-Disposition: form-data; name=\"f\"; filename=\"a.bin\"\r\n\r\n"
                "DDDDDDDDDD\r\n--%s--\r\n", token, token);
    req = sfmt("POST /test/show HTTP/1.1\r\nHost: x\r\n"
               "Content-Type: multipart/form-data; boundary=%s\r\n"
               "Content-Length: %d\r\nConnection: close\r\n\r\n%s", token, (int) slen(body), body);
    segments[0] = req;
    status = rawRequest(endpoint, segments, 1, 0, 1, buf, size);
    rFree(token);
    rFree(body);
    rFree(req);
    return status;
}

/*
    RFC 2046 caps a boundary at 70 characters. Anything longer is refused outright, so no scan
    limit can be reached by a boundary the parser accepted.
 */
static void testOverLongBoundaryIsRefused(void)
{
    Ticks started;
    char  buf[8192];
    int   status;

    status = uploadWithBoundary(HTTP, 70, buf, sizeof(buf));
    teqi(status, 200, "A boundary at the RFC limit must still be accepted");

    started = rGetTicks();
    status = uploadWithBoundary(HTTP, 200, buf, sizeof(buf));
    teqi(status, 400, "A boundary past the RFC limit must be refused");
    ttrue(rGetTicks() - started < 2 * TPS, "The refusal must be prompt");
}

/*
    The original denial of service: a boundary at the part scan limit. The refusal must be prompt
    and the server must still answer the next connection.
 */
static void testBoundaryAtScanLimitDoesNotStall(void)
{
    WebHost *host;
    Ticks   started;
    char    buf[8192];
    int     status;

    if ((host = startTestHost(CONFIG_BIG)) == 0) {
        return;
    }
    started = rGetTicks();
    status = uploadWithBoundary(BIG_ENDPOINT, WEB_BUF_BOOST_16X, buf, sizeof(buf));
    teqi(status, 400, "A boundary at the scan limit must be refused");
    ttrue(rGetTicks() - started < 5 * TPS, "The refusal must not stall the server");

    //  The server must still be answering, which it was not while the loop spun
    status = uploadWithBoundary(BIG_ENDPOINT, 70, buf, sizeof(buf));
    teqi(status, 200, "A following request must still be served");
    stopTestHost(host);
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        testOverLongBoundaryIsRefused();
        testBoundaryAtScanLimitDoesNotStall();
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
