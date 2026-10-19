/*
    methods-config.tst.c - Where the server's accepted-method set comes from

    The method hash is the server's only method gate. It used to be derived from the CORS response
    header "web.headers.Access-Control-Allow-Methods", so setting that header to satisfy a browser
    preflight silently granted PUT and DELETE over the document root to every route without its own
    methods array. It now comes from "web.methods", and PUT and DELETE are never granted host-wide:
    a route must list them.

    Each case runs its own in-process host. Ports 4295-4297 are used by this test alone.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define CORS_ENDPOINT  "http://127.0.0.1:4295"
#define HOST_ENDPOINT  "http://127.0.0.1:4296"
#define ROUTE_ENDPOINT "http://127.0.0.1:4297"

#define CREATED        "site/data/methods-created.txt"

//  The CORS header names every method. It must not widen the server's own gate.
#define CONFIG_CORS                                                        \
    "{web: {documents: './site', listen: ['http://127.0.0.1:4295'],"       \
    " headers: {'Access-Control-Allow-Methods': 'GET, POST, PUT, DELETE, OPTIONS'},"  \
    " routes: [{}]}}"

//  web.methods is the real key, but it must not confer the mutating methods either
#define CONFIG_HOST                                                        \
    "{web: {documents: './site', listen: ['http://127.0.0.1:4296'],"       \
    " methods: 'GET, POST, PUT, DELETE',"                                  \
    " routes: [{}]}}"

//  A route that lists them explicitly is the only way to get them
#define CONFIG_ROUTE                                                       \
    "{web: {documents: './site', listen: ['http://127.0.0.1:4297'],"       \
    " routes: [{match: '/data/', methods: ['GET', 'PUT', 'DELETE', 'OPTIONS']}, {}]}}"

/************************************ Code ************************************/

static int fetch(cchar *endpoint, cchar *method, cchar *path, cchar *body)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);
    status = urlFetch(up, method, SFMT(url, "%s%s", endpoint, path), body, body ? (size_t) -1 : 0, NULL);
    urlFree(up);
    return status;
}

/*
    Check the methods a host grants to a route that carries no methods array of its own.
 */
static void checkDefaults(cchar *endpoint, cchar *why)
{
    unlink(CREATED);
    teqi(fetch(endpoint, "GET", "/data/test1.txt", NULL), 200, "%s: GET must be accepted", why);
    teqi(fetch(endpoint, "HEAD", "/data/test1.txt", NULL), 200, "%s: HEAD must be inferred from GET", why);
    teqi(fetch(endpoint, "PUT", "/data/methods-created.txt", "x"), 405, "%s: PUT must be refused", why);
    ttrue(!rFileExists(CREATED), "%s: a refused PUT must not create the file", why);
    teqi(fetch(endpoint, "DELETE", "/data/test1.txt", NULL), 405, "%s: DELETE must be refused", why);
    ttrue(rFileExists("site/data/test1.txt"), "%s: a refused DELETE must not remove the file", why);
}

static void testCorsHeaderDoesNotWidenTheGate(void)
{
    WebHost *host;

    if ((host = startTestHost(CONFIG_CORS)) == 0) {
        return;
    }
    checkDefaults(CORS_ENDPOINT, "The CORS header");
    stopTestHost(host);
}

static void testHostMethodsDoNotGrantMutations(void)
{
    WebHost *host;

    if ((host = startTestHost(CONFIG_HOST)) == 0) {
        return;
    }
    checkDefaults(HOST_ENDPOINT, "web.methods");
    stopTestHost(host);
}

static void testRouteMethodsStillGrantMutations(void)
{
    WebHost *host;
    Url     *up;
    char    url[256];

    if ((host = startTestHost(CONFIG_ROUTE)) == 0) {
        return;
    }
    /*
        The route's methods array is the gate. OPTIONS reports it on every platform, including one
        where the file handler cannot serve the mutations it admits.
     */
    up = urlAlloc(0);
    teqi(urlFetch(up, "OPTIONS", SFMT(url, "%s/data/test1.txt", ROUTE_ENDPOINT), NULL, 0, NULL), 200,
         "A route that lists OPTIONS must accept it");
    tmatch(urlGetHeader(up, "Access-Control-Allow-Methods"), "DELETE,GET,HEAD,OPTIONS,PUT");
    urlFree(up);

#if ME_WEB_MUTATIONS
    teqi(fetch(ROUTE_ENDPOINT, "PUT", "/data/methods-created.txt", "content"), 201,
         "A route that lists PUT must accept it");
    ttrue(rFileExists(CREATED), "The PUT must create the file");
    teqi(fetch(ROUTE_ENDPOINT, "DELETE", "/data/methods-created.txt", NULL), 204,
         "A route that lists DELETE must accept it");
    ttrue(!rFileExists(CREATED), "The DELETE must remove the file");
#else
    tinfo("Skipped: PUT and DELETE are unavailable on this platform");
#endif
    stopTestHost(host);
    unlink(CREATED);
}

static void fiberMain(void *arg)
{
    testCorsHeaderDoesNotWidenTheGate();
    testHostMethodsDoNotGrantMutations();
    testRouteMethodsStillGrantMutations();
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
