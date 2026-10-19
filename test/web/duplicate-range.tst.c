/*
    duplicate-range.tst.c - A repeated Range or If-Range header is refused

    parseRangeHeader assigns the list head unconditionally, so a second Range header overwrote the
    pointer and orphaned the whole previous list - up to sixteen nodes per header, unrecoverable for
    the life of the process. A request under the shipped 10K header limit could carry hundreds of
    them, so one keep-alive connection leaked tens of megabytes while serving normal-looking
    responses. A repeated If-Range leaked its cloned ETag the same way.

    Content-Length, Transfer-Encoding and Authorization are already refused when repeated; these two
    now are too, which also removes the framing ambiguity of two disagreeing Range headers.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

/************************************ Code ************************************/

static int request(cchar *headers, char *buf, size_t size)
{
    cchar *segments[1];
    char  *req;
    int   status;

    req = sfmt("GET /range-test.txt HTTP/1.1\r\nHost: x\r\n%sConnection: close\r\n\r\n", headers);
    segments[0] = req;
    status = rawRequest(HTTP, segments, 1, 0, 1, buf, size);
    rFree(req);
    return status;
}

static void testDuplicateRangeIsRefused(void)
{
    char buf[8192];
    int  status;

    status = request("Range: bytes=0-9\r\n", buf, sizeof(buf));
    teqi(status, 206, "A single Range header must still be honoured");

    status = request("Range: bytes=0-9\r\nRange: bytes=20-29\r\n", buf, sizeof(buf));
    teqi(status, 400, "A repeated Range header must be refused");

    status = request("Range: bytes=0-1,2-3\r\nRange: bytes=4-5,6-7\r\nRange: bytes=8-9\r\n", buf, sizeof(buf));
    teqi(status, 400, "Every repeat must be refused, not only the second");
}

static void testDuplicateIfRangeIsRefused(void)
{
    char buf[8192];
    int  status;

    status = request("Range: bytes=0-9\r\nIf-Range: \"nomatch\"\r\n", buf, sizeof(buf));
    ttrue(status == 200 || status == 206, "A single If-Range header must still be accepted, got %d", status);

    status = request("Range: bytes=0-9\r\nIf-Range: \"a\"\r\nIf-Range: \"b\"\r\n", buf, sizeof(buf));
    teqi(status, 400, "A repeated If-Range header must be refused");
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        testDuplicateRangeIsRefused();
        testDuplicateIfRangeIsRefused();
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
