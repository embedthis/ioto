/*
    session-limit.tst.c - Session table exhaustion

    The session table is bounded. When it fills, an anonymous session is evicted to make room for
    a login, so an attacker who only ever opens anonymous sessions cannot lock users out; once every
    slot holds an authenticated session the next request that needs a session is refused with 429.
    Runs on an in-process host with a limit of three so the shared server is untouched.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define BASE "http://localhost:4858"

static int  allocCount;
static int  failAlloc;
static void *sessionCache;
static bool sessionCacheFreed;

/************************************ Code ************************************/

static void *testAlloc(size_t size)
{
    void *ptr;

    if (++allocCount == failAlloc) {
        return NULL;
    }
    ptr = malloc(size);
    if (allocCount == 3) {
        sessionCache = ptr;
    }
    return ptr;
}

static void *testRealloc(void *ptr, size_t size)
{
    if (++allocCount == failAlloc) {
        return NULL;
    }
    return realloc(ptr, size);
}

static void testFree(void *ptr)
{
    if (ptr == sessionCache) {
        sessionCacheFreed = 1;
    }
    free(ptr);
}

static void ignoreAllocFailure(int cause, size_t size)
{
    (void) cause;
    (void) size;
}

/*
    Force the session-table insertion allocation to fail after the session and its cache have been
    allocated. The constructor must run the complete session destructor on this error path.
 */
static void testSessionInsertFailureCleanup(void)
{
    WebHost    host = { 0 };
    Web        web = { 0 };
    WebSession *session;
    uint       savedLength;

    host.sessions = rAllocHash(1, 0);
    host.maxSessions = INT_MAX;
    host.sessionTimeout = 60 * TPS;
    host.sessionCookie = "sid";
    web.host = &host;
    web.wroteHeaders = 1;

    /* Make the next insertion grow its bucket table; the fourth allocation is that growth. */
    savedLength = host.sessions->length;
    host.sessions->length = host.sessions->numBuckets;
    allocCount = 0;
    failAlloc = 4;
    sessionCache = NULL;
    sessionCacheFreed = 0;
    rSetMemHandler(ignoreAllocFailure);
    rSetAllocator(testAlloc, testRealloc, testFree);

    session = webCreateSession(&web);
    tnull(session, "A failed session-table insertion must reject session creation");
    ttrue(sessionCacheFreed, "A failed session-table insertion must release the session cache");

    if (!sessionCacheFreed && sessionCache) {
        rFreeHash(sessionCache);
    }
    rSetAllocator(NULL, NULL, NULL);
    rSetMemHandler(NULL);
    host.sessions->length = savedLength;
    rFreeHash(host.sessions);
    rFree(web.error);
}

/*
    Request the path and return the status; copies the session cookie the server set, if any
 */
static int get(cchar *path, cchar *cookie, char *setCookie, size_t size)
{
    Url   *up;
    cchar *value;
    char  url[256], *end;
    int   status;

    up = urlAlloc(0);
    if (cookie && *cookie) {
        status = urlFetch(up, "GET", SFMT(url, BASE "%s", path), NULL, 0, "Cookie: %s\r\n", cookie);
    } else {
        status = urlFetch(up, "GET", SFMT(url, BASE "%s", path), NULL, 0, NULL);
    }
    value = urlGetHeader(up, "Set-Cookie");
    scopy(setCookie, size, value ? value : "");
    if ((end = schr(setCookie, ';')) != 0) {
        *end = '\0';
    }
    urlFree(up);
    return status;
}

static void testSessionLimit(void)
{
    WebHost *host;
    char    anon[2][256], cookie[256], path[128];
    int     i, status;

    host = startTestHost("{ web: { documents: './site', listen: ['http://:4858'], "
                         "limits: { sessions: 3 }, "
                         "auth: { roles: { public: [], user: ['view'] } }, "
                         "routes: [ { match: '/test/session/', handler: 'action' }, { match: '/test/', handler: 'action' }, {} ] } }");
    if (!host) {
        return;
    }
    //  Two anonymous sessions
    for (i = 0; i < 2; i++) {
        status = get("/test/session/create", NULL, anon[i], sizeof(anon[i]));
        teqi(status, 200);
        ttrue(*anon[i] != '\0', "An anonymous request that stores state gets a session");
    }
    //  Three logins fill the table; the anonymous sessions are evicted to make room
    for (i = 0; i < 3; i++) {
        SFMT(path, "/test/session/login?user=user%d&role=user", i);
        status = get(path, NULL, cookie, sizeof(cookie));
        teqi(status, 200, "Login %d must succeed while anonymous sessions can be evicted", i);
        ttrue(*cookie != '\0');
    }
    //  The evicted anonymous sessions are gone: presenting their cookie yields a new session
    for (i = 0; i < 2; i++) {
        status = get("/test/session/getvar?name=token", anon[i], cookie, sizeof(cookie));
        ttrue(status == 429 || (status == 200 && !smatch(cookie, anon[i])),
              "An evicted anonymous session must not be found again");
    }
    //  Every slot is authenticated: nothing can be evicted
    status = get("/test/session/login?user=user9&role=user", NULL, cookie, sizeof(cookie));
    teqi(status, 429, "A login past the limit with no anonymous session to evict must be refused");

    status = get("/test/session/create", NULL, cookie, sizeof(cookie));
    teqi(status, 429, "A new anonymous session past the limit must be refused");

    //  Existing authenticated sessions keep working and requests that need no session are unaffected
    status = get("/index.html", NULL, cookie, sizeof(cookie));
    teqi(status, 200, "A request that needs no session is served");

    stopTestHost(host);
}

static void fiberMain(void *arg)
{
    if (setup(NULL, NULL)) {
        testSessionInsertFailureCleanup();
        testSessionLimit();
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
