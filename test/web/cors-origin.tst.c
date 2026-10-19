/*
    cors-origin.tst.c - webAddAccessControlHeader must not echo an arbitrary request Origin

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define ENDPOINT "http://127.0.0.1:4878"

#define CONFIG                                                              \
    "{web: {documents: './site', listen: ['" ENDPOINT "'],"                 \
    " cors: {origins: ['https://console.example']},"                        \
    " routes: [{match: '/test/', handler: 'action'}, {}]}}"

/************************************ Code ************************************/

static void corsAction(Web *web)
{
    webAddAccessControlHeader(web);
    webWriteResponseString(web, 200, "device-config\n");
}

static void corsVaryAction(Web *web)
{
    webAddHeaderStaticString(web, "Vary", "Accept-Encoding");
    webAddAccessControlHeader(web);
    webWriteResponseString(web, 200, "device-config\n");
}

/*
    Fetch a URI with an optional Origin and return the raw response in buf
 */
static int fetch(cchar *uri, cchar *origin, char *buf, size_t size)
{
    cchar *segments[1];
    char  *req;
    int   status;

    req = sfmt("GET %s HTTP/1.1\r\nHost: 127.0.0.1:4878\r\n%s%s%sConnection: close\r\n\r\n", uri,
               origin ? "Origin: " : "", origin ? origin : "", origin ? "\r\n" : "");
    segments[0] = req;
    status = rawRequest(ENDPOINT, segments, 1, 0, 1, buf, size);
    rFree(req);
    return status;
}


static int header(cchar *buf, cchar *name, char *value, size_t size)
{
    cchar *cp, *end;
    char  line[80];
    int   count;

    SFMT(line, "\r\n%s: ", name);
    value[0] = '\0';
    for (count = 0, cp = buf; (cp = sncaselesscontains(cp, line, slen(cp))) != 0; count++) {
        cp += slen(line);
        if (count == 0 && (end = scontains(cp, "\r\n")) != 0) {
            sncopy(value, size, cp, (size_t) (end - cp));
        }
    }
    return count;
}

static cchar *allowOrigin(cchar *origin, char *value, size_t size)
{
    char buf[8192];

    teqi(fetch("/test/cors", origin, buf, sizeof(buf)), 200);
    header(buf, "Vary", value, size);
    tmatch(value, "Origin");
    header(buf, "Access-Control-Allow-Origin", value, size);
    return value;
}

static void testOriginAllowlist(void)
{
    WebHost *host;
    char    value[256], buf[8192];

    if ((host = startTestHost(CONFIG)) == 0) {
        return;
    }
    webAddAction(host, "/test/cors", corsAction, NULL);
    webAddAction(host, "/test/corsvary", corsVaryAction, NULL);

    tfalse(smatch(allowOrigin("https://evil.example", value, sizeof(value)), "https://evil.example"),
           "An unlisted cross origin must not be echoed");
    tmatch(allowOrigin("https://console.example", value, sizeof(value)), "https://console.example",
           "A listed origin is echoed");
    tmatch(allowOrigin(ENDPOINT, value, sizeof(value)), ENDPOINT, "The same origin is echoed");
    tmatch(allowOrigin(NULL, value, sizeof(value)), ENDPOINT, "No Origin names this host");

    //  Origin is appended to an existing Vary, in one field
    teqi(fetch("/test/corsvary", "https://console.example", buf, sizeof(buf)), 200);
    teqi(header(buf, "Vary", value, sizeof(value)), 1, "A single Vary field");
    tmatch(value, "Accept-Encoding, Origin");
    stopTestHost(host);
}

static void fiberMain(void *arg)
{
    if (setup(NULL, NULL)) {
        testOriginAllowlist();
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
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
