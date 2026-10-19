/*
    path-control-chars.tst.c - Control characters in the decoded request path (web issue 13)

    The encoded URL is allow-listed, so a control byte can only arrive percent-encoded and
    appears once the path is decoded. A newline there splits every log line that names the
    request, which lets an unauthenticated request forge a log record. Any control byte in the
    decoded path must be refused, while ordinary percent-encoding still decodes and serves.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

/************************************ Code ************************************/

/*
    Send the encoded target verbatim. The URL client may reject a control escape before the
    server's 400 response is observable, so it cannot distinguish the fixed and unfixed server.
 */
static int pathProbe(cchar *encodedPath)
{
    cchar *segments[1];
    char  req[1024], buf[8192];

    SFMT(req, "GET %s HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", encodedPath);
    segments[0] = req;
    return rawRequest(HTTP, segments, 1, 0, 1, buf, sizeof(buf));
}

static void testControlCharsInPathRefused(void)
{
    teqi(pathProbe("/x%0afoo"), 400, "Newline (%%0a) in the path must be refused");
    teqi(pathProbe("/x%0dfoo"), 400, "CR (%%0d) in the path must be refused");
    teqi(pathProbe("/x%09foo"), 400, "Tab (%%09) in the path must be refused");
    teqi(pathProbe("/x%01foo"), 400, "A control byte (%%01) in the path must be refused");
    teqi(pathProbe("/x%1bfoo"), 400, "Escape (%%1b) in the path must be refused");
    teqi(pathProbe("/x%7ffoo"), 400, "DEL (%%7f) in the path must be refused");
    teqi(pathProbe("/index.html%0a"), 400, "A trailing newline must be refused");
    teqi(pathProbe("/%0a"), 400, "A path that is only a newline must be refused");
}

/*
    Legitimate percent-encoding still decodes: a UTF-8 name is served and an encoded space is
    looked up as a space (absent here, so 404), not refused.
 */
static void testLegitimateEncodingStillDecodes(void)
{
#if ME_WIN_LIKE
    /*
        The server opens documents through the ANSI file APIs, so the decoded UTF-8 bytes are read
        in the machine's code page and a non-ASCII name is not found. See web issue 41.
     */
    tinfo("Skipped: Non-ASCII document names are unsupported on this platform");
#else
    teqi(pathProbe("/unicode/caf%c3%a9.html"), 200, "UTF-8 percent-encoding must still serve");
#endif
    teqi(pathProbe("/no%20such%20file.html"), 404, "An encoded space is decoded and looked up");
    teqi(pathProbe("/index.html?q=%0a"), 200, "A control byte in the query is data, not a path");
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        testControlCharsInPathRefused();
        testLegitimateEncodingStillDecodes();
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
