/*
    route-segment.tst.c - A route pattern without a trailing "/" covers its subtree

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define SEGMENT "http://127.0.0.1:4891"
#define ROOT    "http://127.0.0.1:4892"

/************************************ Code ************************************/

static int fetch(cchar *base, cchar *path)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s%s", base, path), NULL, 0, NULL);
    urlFree(up);
    return status;
}

static void testSlashlessRouteCoversSubtree(void)
{
    WebHost *host;

    host = startTestHost("{ web: { documents: './site', listen: ['" SEGMENT "'], "
                         "auth: { roles: { public: [], user: ['view'], admin: ['user'] } }, "
                         "routes: [ { match: '/admin', role: 'admin' }, {} ] } }");
    if (!host) {
        return;
    }
    teqi(fetch(SEGMENT, "/admin"), 401, "The route's own URL is protected");
    teqi(fetch(SEGMENT, "/admin/"), 401, "The route's directory URL is protected");
    teqi(fetch(SEGMENT, "/admin/secret.html"), 401, "Content beneath a slashless role route is protected");


    teqi(fetch(SEGMENT, "/adminx"), 404, "A sibling path sharing the prefix is not matched");
    teqi(fetch(SEGMENT, "/index.html"), 200, "The catch-all still serves public content");
    stopTestHost(host);
}

static void testRootRouteIsExact(void)
{
    WebHost *host;

    host = startTestHost("{ web: { documents: './site', listen: ['" ROOT "'], "
                         "auth: { roles: { public: [], user: ['view'], admin: ['user'] } }, "
                         "routes: [ { match: '/', role: 'admin' }, {} ] } }");
    if (!host) {
        return;
    }
    teqi(fetch(ROOT, "/"), 401, "The root route protects the root");
    teqi(fetch(ROOT, "/index.html"), 200, "The root route does not capture the whole tree");
    stopTestHost(host);
}

static void fiberMain(void *arg)
{
    testSlashlessRouteCoversSubtree();
    testRootRouteIsExact();
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
