/*
    cookie-delimiters.tst.c - A Cookie header of nothing but delimiters

    Both cookie parsers tokenise on ';' and then on '='. stok returns NULL and clears its saved
    position when the whole remaining string is delimiters, and passing that NULL to the next stok
    makes it resume from a local nothing has assigned - a read through an uninitialised pointer, and
    a NUL write through it when the residue happens to contain a delimiter. A nine-byte header from
    an unauthenticated client was enough: on builds with fiber exception blocks the request was
    dropped with no response, and without them the process faulted.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

/************************************ Code ************************************/

/*
    Send a request to a session-authorized route carrying the given Cookie header, and require a
    well-formed answer rather than a dropped connection.
 */
static void checkCookie(cchar *cookie, cchar *why)
{
    cchar *segments[1];
    char  *req, buf[8192];
    int   status;

    req = sfmt("GET /test/admin/show HTTP/1.1\r\nHost: x\r\nCookie: %s\r\nConnection: close\r\n\r\n", cookie);
    segments[0] = req;
    status = rawRequest(HTTP, segments, 1, 0, 1, buf, sizeof(buf));
    rFree(req);

    teqi(status, 401, "%s must be answered, not dropped", why);
    ttrue(sstarts(buf, "HTTP/1.1 "), "%s must produce a well-formed response, got '%.16s'", why, buf);
}

static void testDelimiterOnlyCookies(void)
{
    //  The shapes that made stok hand a NULL to the next stok
    checkCookie(";", "A single semicolon");
    checkCookie(";;;;", "A run of semicolons");
    checkCookie(";;", "A pair of semicolons");

    //  Neighbours that always produced a token, as controls
    checkCookie("; ; ;", "Semicolons separated by spaces");
    checkCookie("-web-session-=abc", "A well-formed but unknown session cookie");
    checkCookie("a=1; b=2", "Two well-formed cookies");
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        testDelimiterOnlyCookies();
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
