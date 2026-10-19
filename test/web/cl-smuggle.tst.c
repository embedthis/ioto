/*
    cl-smuggle.tst.c - Content-Length hardening (web issue 14)

    RFC 9112 6.3 defines Content-Length as digits only, and a request with two differing values
    must be refused. A lenient parser that accepts "5, 10", "5abc", "+5" or a differing duplicate
    can be framed differently by a front-end than by this server, which is the request-smuggling
    primitive. These run over a raw socket because a client library cannot express them.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

/************************************ Code ************************************/

/*
    POST /test/show with the caller's Content-Length header block and a 5-byte body
 */
static int clProbe(cchar *clHeaders)
{
    cchar *segments[1];
    char  req[1024], buf[8192];

    SFMT(req, "POST /test/show HTTP/1.1\r\nHost: x\r\nConnection: close\r\n%s\r\n\r\nHELLO", clHeaders);
    segments[0] = req;
    return rawRequest(HTTP, segments, 1, 0, 1, buf, sizeof(buf));
}

static void testContentLengthHardening(void)
{
    teqi(clProbe("Content-Length: 5"), 200, "A well-formed Content-Length must still succeed");
    teqi(clProbe("Content-Length: 5\r\nContent-Length: 5"), 200, "Repeated identical values are tolerated");
    teqi(clProbe("Content-Length: 5, 10"), 400, "A comma list must be refused");
    teqi(clProbe("Content-Length: 5abc"), 400, "Trailing garbage must be refused");
    teqi(clProbe("Content-Length: +5"), 400, "A signed value must be refused");
    teqi(clProbe("Content-Length: -5"), 400, "A negative value must be refused");
    teqi(clProbe("Content-Length: 0x5"), 400, "A hex value must be refused");
    teqi(clProbe("Content-Length: "), 400, "An empty value must be refused");
    teqi(clProbe("Content-Length: 10\r\nContent-Length: 5"), 400, "Differing duplicates must be refused");
    teqi(clProbe("Content-Length: 99999999999999999999"), 400, "An overflowing value must be refused");
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        testContentLengthHardening();
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
