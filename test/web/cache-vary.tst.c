/*
    cache-vary.tst.c - Cache-Control and Vary describe what a shared cache may store

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define ENDPOINT "http://127.0.0.1:4862"

/************************************ Code ************************************/

/*
    Send a raw request and return the status. The response is left in buf.
 */
static int request(cchar *req, char *buf, size_t size)
{
    cchar *segments[1];

    segments[0] = req;
    return rawRequest(ENDPOINT, segments, 1, 0, 1, buf, size);
}

/*
    Copy the value of a response header into value. Returns false if absent.
 */
static bool header(cchar *response, cchar *name, char *value, size_t size)
{
    cchar *cp, *end;
    char  key[64];

    SFMT(key, "\r\n%s:", name);
    if ((cp = sncaselesscontains(response, key, slen(response))) == 0) {
        *value = '\0';
        return 0;
    }
    cp += slen(key);
    while (*cp == ' ') {
        cp++;
    }
    if ((end = scontains(cp, "\r\n")) == 0) {
        *value = '\0';
        return 0;
    }
    sncopy(value, size, cp, (size_t) (end - cp));
    return 1;
}

static int count(cchar *response, cchar *name)
{
    cchar *cp;
    char  key[64];
    int   n;

    SFMT(key, "\r\n%s:", name);
    for (n = 0, cp = response; (cp = sncaselesscontains(cp, key, slen(cp))) != 0; cp += slen(key)) {
        n++;
    }
    return n;
}

static void testVaryMerged(void)
{
    char buf[8192], req[512], vary[256], etag[64], vary304[256];

    //  An identity response on a compressed route still depends on Accept-Encoding
    teqi(request("GET /compressed/uncompressed.txt HTTP/1.1\r\nHost: x\r\n\r\n", buf, sizeof(buf)), 200);
    header(buf, "Vary", vary, sizeof(vary));
    tcontains(vary, "Accept-Encoding");

    //  The encoded variant keys on Accept-Encoding and the route's Cookie and Authorization in one Vary
    teqi(request("GET /compressed/data.json HTTP/1.1\r\nHost: x\r\nAccept-Encoding: gzip\r\n\r\n", buf, sizeof(buf)), 200);
    tcontains(buf, "Content-Encoding: gzip");
    teqi(count(buf, "Vary"), 1, "One Vary field");
    header(buf, "Vary", vary, sizeof(vary));
    tcontains(vary, "Accept-Encoding");
    tcontains(vary, "Cookie");
    tcontains(vary, "Authorization");
    tfalse(scontains(vary, "Origin") != NULL, "Compression does not add Origin");

    //  The 304 carries the same Vary as the 200 it validates
    header(buf, "ETag", etag, sizeof(etag));
    SFMT(req, "GET /compressed/data.json HTTP/1.1\r\nHost: x\r\nAccept-Encoding: gzip\r\nIf-None-Match: %s\r\n\r\n", etag);
    teqi(request(req, buf, sizeof(buf)), 304);
    header(buf, "Vary", vary304, sizeof(vary304));
    tmatch(vary304, vary);
}

static void testCredentialedNotStored(void)
{
    char buf[8192], req[512], value[256], cookie[256], *semi;

    //  A body bound to an existing session, with no Set-Cookie on this response
    teqi(request("GET /test/session/create HTTP/1.1\r\nHost: x\r\n\r\n", buf, sizeof(buf)), 200);
    ttrue(header(buf, "Set-Cookie", cookie, sizeof(cookie)), "The session is created");
    if ((semi = schr(cookie, ';')) != 0) {
        *semi = '\0';
    }
    SFMT(req, "GET /test/session/getvar?name=token HTTP/1.1\r\nHost: x\r\nCookie: %s\r\n\r\n", cookie);
    teqi(request(req, buf, sizeof(buf)), 200);
    tfalse(header(buf, "Set-Cookie", value, sizeof(value)), "No cookie is minted for an existing session");
    header(buf, "Cache-Control", value, sizeof(value));
    tmatch(value, "no-store", "A session-bound body must not be stored");

    //  A response carrying an XSRF token
    teqi(request("GET /test/xsrf/ HTTP/1.1\r\nHost: x\r\n\r\n", buf, sizeof(buf)), 200);
    ttrue(header(buf, "X-XSRF-TOKEN", value, sizeof(value)), "The response carries a token");
    header(buf, "Cache-Control", value, sizeof(value));
    tmatch(value, "no-store", "A token-carrying body must not be stored");
}

static void testHandlerCacheControlWins(void)
{
    char buf[8192], value[256];

    //  The form action sets its own Cache-Control on a route with a public policy
    teqi(request("GET /test/form HTTP/1.1\r\nHost: x\r\n\r\n", buf, sizeof(buf)), 200);
    teqi(count(buf, "Cache-Control"), 1, "One Cache-Control field");
    header(buf, "Cache-Control", value, sizeof(value));
    tmatch(value, "no-cache", "The handler's policy is not overlaid with the route's");
}

static void testPrivateHasNoExpires(void)
{
    char buf[8192], value[256];

    teqi(request("GET /index.html HTTP/1.0\r\n\r\n", buf, sizeof(buf)), 200);
    header(buf, "Cache-Control", value, sizeof(value));
    tcontains(value, "private");
    tfalse(header(buf, "Expires", value, sizeof(value)), "An HTTP/1.0 cache does not understand private");
}

static void fiberMain(void *arg)
{
    WebHost *host;

    if (!setup(NULL, NULL)) {
        rStop();
        return;
    }
    host = startTestHost("{ web: { documents: './site/', listen: ['" ENDPOINT "'], "
                         "routes: ["
                         "  { match: '/compressed/', handler: 'file', compressed: true, cache: { maxAge: '1hr', directives: 'public' } },"
                         "  { match: '/test/xsrf/', handler: 'action', xsrf: true, cache: { maxAge: '1hr', directives: 'public' } },"
                         "  { match: '/test/', handler: 'action', cache: { maxAge: '1hr', directives: 'public' } },"
                         "  { match: '/index.html', handler: 'file', cache: { maxAge: '1hr', directives: 'private' } },"
                         "  {} ] } }");
    if (host) {
        testVaryMerged();
        testCredentialedNotStored();
        testHandlerCacheControlWins();
        testPrivateHasNoExpires();
        stopTestHost(host);
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
