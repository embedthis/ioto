/*
    session-lifetime.tst.c - A session freed under an in-flight request must not be read

    web->session was a borrowed pointer into host->sessions, and the three paths that free a
    WebSession - logout (and so every login), expiry pruning and anonymous eviction - cannot see the
    borrows held by requests already in flight. Ioto is cooperatively scheduled and a request yields
    in webReadBody after the session has been resolved, so the session could be freed under it and
    every later webGetSession would read and write freed memory.

    Both cases park an authenticated request mid-body, free its session from another connection, and
    then complete the body. Without the fix these are use-after-free reads, which is only reliably
    visible under a sanitizer, so this test is written to be run by bin/sanitize.sh as well as
    normally: it asserts the parked request still gets a well-formed response either way.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

#define PRUNE_ENDPOINT "http://127.0.0.1:4863"

//  Sessions expire in a second and the table holds two, so an anonymous burst prunes the victim
#define CONFIG_PRUNE                                                        \
    "{web: {documents: './site', listen: ['http://127.0.0.1:4863'],"        \
    " limits: {sessions: 2}, timeouts: {session: '1sec'},"                  \
    " auth: {roles: {public: [], user: ['view'], admin: ['user']}},"        \
    " routes: ["                                                            \
    "  {match: '/test/admin/', handler: 'action', role: 'admin'},"          \
    "  {match: '/test/', handler: 'action'},"                               \
    "  {}"                                                                  \
    " ]}}"

/************************************ Code ************************************/

/*
    Log in and return the session cookie, or NULL. The caller frees it.
 */
static char *login(cchar *endpoint)
{
    Url   *up;
    cchar *header;
    char  url[256], *cookie, *end;

    up = urlAlloc(0);
    if (urlFetch(up, "GET", SFMT(url, "%s/test/session/login?user=alice&role=admin", endpoint), NULL, 0, NULL) != 200) {
        urlFree(up);
        return 0;
    }
    if ((header = urlGetHeader(up, "Set-Cookie")) == 0) {
        urlFree(up);
        return 0;
    }
    cookie = sclone(header);
    if ((end = schr(cookie, ';')) != 0) {
        *end = '\0';
    }
    urlFree(up);
    return cookie;
}

/*
    Open a connection, send an authenticated POST header block and part of its body, and leave the
    server fiber parked in webReadBody with the session already resolved by the route's role gate.
 */
static RSocket *parkRequest(cchar *endpoint, cchar *cookie)
{
    RSocket *sp;
    char    req[1024];

    if ((sp = rawConnect(endpoint)) == 0) {
        tfail("Cannot connect to %s", endpoint);
        return 0;
    }
    SFMT(req, "POST /test/admin/show HTTP/1.1\r\nHost: x\r\nCookie: %s\r\n"
         "Content-Type: text/plain\r\nContent-Length: 20\r\nConnection: close\r\n\r\nAAAAA", cookie);
    if (rWriteSocket(sp, req, slen(req), rGetTicks() + 5 * TPS) < 0) {
        tfail("Cannot write the parked request");
        rFreeSocket(sp);
        return 0;
    }
    //  Let the server read the headers and block on the body
    rSleep(200);
    return sp;
}

/*
    Send the rest of the body and require a well-formed response.
 */
static void completeRequest(RSocket *sp, cchar *why)
{
    char buf[16384];

    if (rWriteSocket(sp, "BBBBBBBBBBBBBBB", 15, rGetTicks() + 5 * TPS) < 0) {
        tfail("%s: cannot complete the parked request", why);
        rFreeSocket(sp);
        return;
    }
    buf[0] = '\0';
    rawRead(sp, buf, sizeof(buf), 1);
    ttrue(sstarts(buf, "HTTP/1.1 "), "%s: the parked request must still get a well-formed response, got '%.16s'",
          why, buf);
    rFreeSocket(sp);
}

/*
    Free path 1: a second login destroys the session the parked request resolved.
 */
static void testLoginDestroysAnInFlightSession(void)
{
    RSocket *sp;
    Url     *up;
    char    *cookie, url[256];

    if ((cookie = login(HTTP)) == 0) {
        tfail("Cannot log in");
        return;
    }
    if ((sp = parkRequest(HTTP, cookie)) == 0) {
        rFree(cookie);
        return;
    }
    //  Logging in again on the same session destroys it
    up = urlAlloc(0);
    teqi(urlFetch(up, "GET", SFMT(url, "%s/test/session/login?user=alice&role=admin", HTTP), NULL, 0,
                  "Cookie: %s\r\n", cookie), 200, "The second login must succeed");
    urlFree(up);

    completeRequest(sp, "A session destroyed by a second login");
    rFree(cookie);
}

/*
    Free path 2: expiry pruning, driven by unauthenticated traffic filling the session table.
 */
static void testPruneFreesAnInFlightSession(void)
{
    WebHost *host;
    RSocket *sp;
    Url     *up;
    char    *cookie, url[256];
    int     i;

    if ((host = startTestHost(CONFIG_PRUNE)) == 0) {
        return;
    }
    if ((cookie = login(PRUNE_ENDPOINT)) == 0) {
        tfail("Cannot log in to the prune host");
        stopTestHost(host);
        return;
    }
    if ((sp = parkRequest(PRUNE_ENDPOINT, cookie)) == 0) {
        rFree(cookie);
        stopTestHost(host);
        return;
    }
    //  Let the parked session pass its one-second expiry, then fill the table anonymously
    rSleep(1500);
    up = urlAlloc(0);
    for (i = 0; i < 3; i++) {
        urlFetch(up, "GET", SFMT(url, "%s/test/session/create", PRUNE_ENDPOINT), NULL, 0, NULL);
        urlClose(up);
    }
    urlFree(up);

    completeRequest(sp, "A session freed by expiry pruning");
    rFree(cookie);
    stopTestHost(host);
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        testLoginDestroysAnInFlightSession();
        testPruneFreesAnInFlightSession();
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
