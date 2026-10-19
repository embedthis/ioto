/*
    content-length-width.tst.c - A Content-Length past 32 bits must be refused on any word size

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

/************************************ Code ************************************/

static void testWideContentLength(void)
{
    cchar *values[] = { "2147483648", "4294967296", "4294967396", 0 };
    cchar *segments[1];
    char  buf[8192], *req;
    int   i, status;

    for (i = 0; values[i]; i++) {
        req = sfmt("POST /test/show HTTP/1.1\r\nHost: x\r\nContent-Length: %s\r\n\r\n"
                   "GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", values[i]);
        segments[0] = req;
        status = rawRequest(HTTP, segments, 1, 0, 2, buf, sizeof(buf));
        ttrue(status == 400 || status == 413, "Content-Length %s must be refused, got %d", values[i], status);
        teqi(rawCountResponses(buf, (ssize) slen(buf)), 1, "The body bytes must not be answered as a request");
        rFree(req);
    }
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        testWideContentLength();
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
