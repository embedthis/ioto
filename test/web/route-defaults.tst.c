/*
    route-defaults.tst.c - A host with no route table fails closed

    When web.routes was absent, initRoutes installed a catch-all file route with no role, so a host
    whose route table failed to load - a config read from somewhere the loader did not look, a
    listener supplied on the command line - served the whole document root to anonymous clients.
    The failure was silent: nothing warned about the missing table, and checkRouteKeys never ran
    because there were no routes to check.

    The double-nested "web.web" shape has the same effect, because everything under it is invisible.

    Ports 4870-4871 are used by this test alone.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define NOROUTES "http://127.0.0.1:4870"
#define DECLARED "http://127.0.0.1:4871"

/************************************ Code ************************************/

static int fetch(cchar *endpoint)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", endpoint), NULL, 0, NULL);
    urlFree(up);
    return status;
}

/*
    No route table: the document root must not be anonymously reachable.
 */
static void testNoRoutesDeniesAnonymous(void)
{
    WebHost *host;

    host = startTestHost("{ web: { documents: './site', listen: ['" NOROUTES "'], "
                         "auth: { roles: { public: [], user: ['view'], admin: ['user'] } } } }");
    if (!host) {
        return;
    }
    teqi(fetch(NOROUTES), 401, "A host with no route table must not serve the document root anonymously");
    stopTestHost(host);
}

/*
    A declared catch-all is the way to ask for an unauthenticated document root, and must be
    unaffected.
 */
static void testDeclaredCatchAllStillServes(void)
{
    WebHost *host;

    host = startTestHost("{ web: { documents: './site', routes: [{}], listen: ['" DECLARED "'] } }");
    if (!host) {
        return;
    }
    teqi(fetch(DECLARED), 200, "A declared catch-all must still serve anonymously");
    stopTestHost(host);
}

/*
    A whole web.json5 pasted under another "web" key hides every setting below it, including the
    route table. The host still comes up - on the fail-closed catch-all - and must say why.
 */
static void testNestedWebNodeIsReported(void)
{
    WebHost *host;

    host = startTestHost("{ web: { web: { documents: './site', routes: [{}] }, "
                         "documents: './site', listen: ['" DECLARED "'], "
                         "auth: { roles: { public: [], user: ['view'], admin: ['user'] } } } }");
    if (!host) {
        return;
    }
    //  The nested routes are invisible, so the fail-closed catch-all is what answers
    teqi(fetch(DECLARED), 401, "A nested web.web node must not leave the document root open");
    stopTestHost(host);
}

static void fiberMain(void *arg)
{
    testNoRoutesDeniesAnonymous();
    testDeclaredCatchAllStillServes();
    testNestedWebNodeIsReported();
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
