/*
    session-exhaustion.tst.c - Anonymous traffic must not lock out logins

    Sessions are created before authentication: webAddSecurityToken creates one to hand an anonymous
    client an XSRF token. If unauthenticated sessions can fill the table, a stream of anonymous GETs
    denies the login path to everyone - a remote, unauthenticated denial of service against the one
    function that matters most.

    When the table is full a new session must still be creatable by evicting the least recently used
    session that has not authenticated. A session that has authenticated must never be evicted this
    way.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTPS;

/*
    Comfortably more anonymous sessions than web.json5's configured limit (200), so the table is
    driven past full rather than merely up to it.

    The flood reuses a single keep-alive connection. The client does not replay Set-Cookie, so each
    request still creates a fresh session, but the server sees two connections rather than several
    hundred. That matters: this suite shares one server with every other test, and opening hundreds
    of connections starves them of the configured connection limit and fails them for reasons that
    have nothing to do with what they assert.
 */
#define FLOOD 250

/************************************ Code ************************************/

/*
    Drive the session table past its limit with anonymous requests, then confirm a fresh anonymous
    client can still obtain a session. Before the fix this returned 429 once the table filled.
 */
static void testAnonymousFloodDoesNotDenyService(void)
{
    Url   *up;
    cchar *cookie;
    char  url[256];
    int   i, status, refused;

    refused = 0;
    up = urlAlloc(0);
    for (i = 0; i < FLOOD; i++) {
        status = urlFetch(up, "GET", SFMT(url, "%s/test/session/create", HTTPS), NULL, 0, NULL);
        if (status == 429) {
            refused++;
        }
    }
    urlFree(up);
    teqi(refused, 0);

    //  A fresh client after the flood must still be served
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/test/session/create", HTTPS), NULL, 0, NULL);
    teqi(status, 200);
    cookie = urlGetHeader(up, "Set-Cookie");
    tnotnull(cookie);
    urlFree(up);
}

/*
    The property that matters: an authenticated session must survive an anonymous flood. This is the
    difference between evicting the oldest session and evicting the oldest *anonymous* session.
 */
static void testAuthenticatedSessionSurvivesFlood(void)
{
    Url   *up;
    char  *cookie;
    cchar *response;
    char  url[256];
    int   i, status;

    //  Establish an authenticated session and keep its cookie
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/test/session/login?user=alice&role=user", HTTPS),
                      NULL, 0, NULL);
    teqi(status, 200);
    response = urlGetResponse(up);
    tmatch(response, "alice");
    cookie = sclone(urlGetHeader(up, WEB_SESSION_COOKIE));
    if (!cookie) {
        cookie = sclone(urlGetHeader(up, "Set-Cookie"));
    }
    tnotnull(cookie);
    urlFree(up);
    if (!cookie) {
        return;
    }
    //  Extract just the cookie value
    if (scontains(cookie, "=")) {
        char *val = stok(cookie, "=", &val) ? val : cookie;
        char *end = schr(val, ';');
        if (end) {
            *end = '\0';
        }
        //  Flood the table with anonymous sessions over one reused connection
        up = urlAlloc(0);
        for (i = 0; i < FLOOD; i++) {
            urlFetch(up, "GET", SFMT(url, "%s/test/session/create", HTTPS), NULL, 0, NULL);
        }
        urlFree(up);
        //  The authenticated session must still resolve to its stored username
        up = urlAlloc(0);
        status = urlFetch(up, "GET", SFMT(url, "%s/test/session/getvar?name=%s", HTTPS,
                                          WEB_SESSION_USERNAME), NULL, 0,
                          "Cookie: %s=%s\r\n", WEB_SESSION_COOKIE, val);
        teqi(status, 200);
        response = urlGetResponse(up);
        tmatch(response, "alice");
        urlFree(up);
    }
    rFree(cookie);
}

static void fiberMain(void *arg)
{
    if (setup(NULL, &HTTPS)) {
        testAnonymousFloodDoesNotDenyService();
        testAuthenticatedSessionSurvivesFlood();
    }
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
