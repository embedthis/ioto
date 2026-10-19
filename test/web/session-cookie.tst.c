/*
    session-cookie.tst.c - Login replaces the session cookie; logout deletes it

    webCreateSession destroyed the old session first, and that emitted a clearing Set-Cookie for the
    same name as the replacement written immediately after. Same-named response headers do not reach
    the wire in insertion order, so the clear arrived last and won: the client was logged out while
    the server kept an authenticated session nothing could reach or evict. Repeating the flow filled
    the session table and every login then failed with 429.

    The clearing cookie was also not a deletion - a zero lifespan selected the configured session
    timeout - so it persisted an empty value for the whole session lifetime.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

#define ENDPOINT "http://127.0.0.1:4864"

//  Three slots: a login that leaks a slot exhausts the table within a handful of requests
#define CONFIG                                                              \
    "{web: {documents: './site', listen: ['http://127.0.0.1:4864'],"        \
    " limits: {sessions: 3},"                                               \
    " auth: {roles: {public: [], user: ['view'], admin: ['user']}},"        \
    " routes: ["                                                            \
    "  {match: '/test/admin/', handler: 'action', role: 'admin'},"          \
    "  {match: '/test/', handler: 'action'},"                               \
    "  {}"                                                                  \
    " ]}}"

/************************************ Code ************************************/

/*
    Fetch with an optional cookie. The session cookie the response sets is copied into "cookie",
    which is left untouched when the response sets none.
 */
static int fetch(cchar *path, char *cookie, size_t size)
{
    Url   *up;
    cchar *header, *value;
    char  url[256], *end;
    int   status;

    up = urlAlloc(0);
    if (cookie && *cookie) {
        status = urlFetch(up, "GET", SFMT(url, "%s%s", ENDPOINT, path), NULL, 0, "Cookie: %s\r\n", cookie);
    } else {
        status = urlFetch(up, "GET", SFMT(url, "%s%s", ENDPOINT, path), NULL, 0, NULL);
    }
    if ((header = urlGetHeader(up, "Set-Cookie")) != 0) {
        value = header;
        scopy(cookie, size, value);
        if ((end = schr(cookie, ';')) != 0) {
            *end = '\0';
        }
    }
    urlFree(up);
    return status;
}

/*
    A login by a client that already holds a session cookie must leave that client authenticated.
 */
static void testLoginOnAHeldCookie(void)
{
    WebHost *host;
    char    cookie[512];

    if ((host = startTestHost(CONFIG)) == 0) {
        return;
    }
    cookie[0] = '\0';

    //  An anonymous request that creates a session, as the XSRF-then-login flow does
    teqi(fetch("/test/session/create", cookie, sizeof(cookie)), 200, "The anonymous request must succeed");
    ttrue(schr(cookie, '=') && !sends(cookie, "="), "The anonymous request must set a session cookie");

    teqi(fetch("/test/session/login?user=alice&role=admin", cookie, sizeof(cookie)), 200, "The login must succeed");
    ttrue(!sends(cookie, "="), "A login must not leave the client with an emptied session cookie");

    teqi(fetch("/test/admin/show", cookie, sizeof(cookie)), 200,
         "The client must be authenticated after logging in over a held cookie");
    stopTestHost(host);
}

/*
    Each login replaced the session but leaked the old one, which is authenticated and so is never
    evicted and not pruned until the session timeout. Six logins over a three-slot table used to end
    in 429 for everyone.
 */
static void testRepeatedLoginsDoNotLeakSlots(void)
{
    WebHost *host;
    char    cookie[512];
    int     i;

    if ((host = startTestHost(CONFIG)) == 0) {
        return;
    }
    cookie[0] = '\0';
    for (i = 0; i < 6; i++) {
        teqi(fetch("/test/session/login?user=alice&role=admin", cookie, sizeof(cookie)), 200,
             "Login %d must succeed", i);
    }
    teqi(fetch("/test/admin/show", cookie, sizeof(cookie)), 200, "The last login must still be usable");
    stopTestHost(host);
}

/*
    Logout must delete the cookie, not store an empty one for the session lifetime.
 */
static void testLogoutDeletesTheCookie(void)
{
    cchar *segments[1];
    char  cookie[512], req[1024], buf[8192], *start, *end;
    int   status;

    //  Driven entirely over raw connections against the shared server
    segments[0] = "GET /test/session/login?user=alice&role=admin HTTP/1.1\r\nHost: x\r\n"
                  "Connection: close\r\n\r\n";
    status = rawRequest(HTTP, segments, 1, 0, 1, buf, sizeof(buf));
    teqi(status, 200, "The login must succeed");
    if ((start = sncaselesscontains(buf, "Set-Cookie: ", slen(buf))) == 0) {
        tfail("The login must set a session cookie");
        return;
    }
    start += slen("Set-Cookie: ");
    scopy(cookie, sizeof(cookie), start);
    if ((end = schr(cookie, ';')) != 0) {
        *end = '\0';
    }

    //  Read the response off the wire: the deletion is in the Set-Cookie attributes, not the value
    SFMT(req, "GET /test/session/logout HTTP/1.1\r\nHost: x\r\nCookie: %s\r\nConnection: close\r\n\r\n", cookie);
    segments[0] = req;
    status = rawRequest(HTTP, segments, 1, 0, 1, buf, sizeof(buf));
    teqi(status, 200, "The logout must succeed");
    tcontains(buf, "Max-Age=0");
    tcontains(buf, "Expires=Thu, 01 Jan 1970");
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        testLoginOnAHeldCookie();
        testRepeatedLoginsDoNotLeakSlots();
        testLogoutDeletesTheCookie();
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
