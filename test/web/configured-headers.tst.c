/*
    configured-headers.tst.c - Response headers supplied by the host configuration

    The agent ships a headers block (its CORS policy among them). A configured header must
    appear on every response the server writes: a file, an action, an error, a HEAD, over TLS.
    A configured header overrides the code baseline for that name; a baseline header the
    configuration does not name is still emitted; values pass through verbatim.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void checkHeaders(cchar *method, cchar *url, int expected)
{
    Url   *up;
    cchar *value;
    int   status;

    up = urlAlloc(0);
    status = urlFetch(up, method, url, NULL, 0, NULL);
    teqi(status, expected, "%s %s", method, url);

    value = urlGetHeader(up, "X-Test-Config");
    tmatch(value, "from-web-json5; q=1");

    //  Configured value replaces the baseline SAMEORIGIN
    value = urlGetHeader(up, "X-Frame-Options");
    tmatch(value, "DENY");

    //  Baseline header the configuration does not name
    value = urlGetHeader(up, "X-Content-Type-Options");
    tmatch(value, "nosniff");

    value = urlGetHeader(up, "Referrer-Policy");
    tmatch(value, "same-origin");
    urlFree(up);
}

static void testConfiguredHeadersOnEveryResponse(void)
{
    char url[128];

    checkHeaders("GET", SFMT(url, "%s/index.html", HTTP), 200);
    checkHeaders("HEAD", SFMT(url, "%s/trace/index.html", HTTP), 200);
    checkHeaders("GET", SFMT(url, "%s/test/show", HTTP), 200);
    checkHeaders("GET", SFMT(url, "%s/does-not-exist.html", HTTP), 404);
    checkHeaders("GET", SFMT(url, "%s/index.html", HTTPS), 200);
}

/*
    A header is emitted once even when configured and in the baseline
 */
static void testConfiguredHeaderEmittedOnce(void)
{
    cchar *segments[1];
    char  buf[8192], *first, *second;
    int   status;

    segments[0] = "GET /index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n";
    status = rawRequest(HTTP, segments, 1, 0, 1, buf, sizeof(buf));
    teqi(status, 200);
    first = sncaselesscontains(buf, "X-Frame-Options:", slen(buf));
    tnotnull(first);
    if (first) {
        second = sncaselesscontains(&first[1], "X-Frame-Options:", slen(&first[1]));
        ttrue(second == NULL, "A configured header must not be emitted twice");
    }
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, &HTTPS)) {
        testConfiguredHeadersOnEveryResponse();
        testConfiguredHeaderEmittedOnce();
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
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
