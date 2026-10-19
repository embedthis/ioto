/*
    host-config.tst.c - Unit tests for WebHost configuration parsing

    These build hosts from JSON5 configuration strings and inspect the parsed WebHost, covering
    the route, redirect and diagnostic paths that the shared test/web.json5 does not reach.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

/*
    Build a host from a JSON5 configuration string. The host takes ownership of the parsed
    config, so webFreeHost releases it.
 */
static WebHost *hostFromConfig(cchar *text)
{
    WebHost *host;
    Json    *config;

    if ((config = jsonParse(text, 0)) == 0) {
        tfail("Cannot parse test configuration");
        return 0;
    }
    if ((host = webAllocHost(config, 0)) == 0) {
        tfail("Cannot allocate host");
        jsonFree(config);
    }
    return host;
}

static WebRoute *routeAt(WebHost *host, int index)
{
    return (WebRoute*) rGetItem(host->routes, index);
}

/*
    Invalid negative timeouts must fall back to their documented defaults. A one-second digest
    lifetime is valid, but its cleanup timer must still be scheduled in the future rather than as
    an immediate event that continuously re-arms itself.
 */
static void testTimeoutBounds(void)
{
    WebHost *host;
    Ticks   delay;

    host = hostFromConfig("{web: {documents: './site', timeouts: {parse: '-1', inactivity: '-1', "
                          "request: '-1', session: '-1'}}}");
    if (host) {
        teqll(host->parseTimeout, 5 * TPS, "Negative parse timeout uses its default");
        teqll(host->inactivityTimeout, 5 * 60 * TPS, "Negative inactivity timeout uses its default");
        teqll(host->requestTimeout, 5 * 60 * TPS, "Negative request timeout uses its default");
        teqll(host->sessionTimeout, 30 * 60 * TPS, "Negative session timeout uses its default");
        webFreeHost(host);
    }

    host = hostFromConfig("{web: {documents: './site', timeouts: {request: 'infinite'}}}");
    if (host) {
        teqll(host->requestTimeout, (MAXINT64 / TPS) * TPS,
             "An infinite timeout is clamped without overflowing");
        webFreeHost(host);
    }

    host = hostFromConfig("{web: {documents: './site', timeouts: {digest: '1'}}}");
    if (host) {
        delay = rGetNextDueEvent() - rGetTicks();
        ttrue(delay >= TPS / 2, "Digest cleanup must never be scheduled with a zero period");
        webFreeHost(host);
    }
}



static void testAllocFailureFreesHost(void)
{
    Json *config;

    config = jsonParse("{web: {documents: './site', signatures: {enable: true, path: 'no-such-signatures.json5'}}}", 0);
    tnull(webAllocHost(config, 0), "A missing signatures file refuses the host");
    jsonFree(config);
}

static void testLimitBounds(void)
{
    WebHost  *host;
    WebRoute *route;

    host = hostFromConfig("{web: {documents: './site', limits: {header: '-1', body: '-5', buffer: '-1', "
                          "maxFrame: '-1', maxMessage: '-1'}, "
                          "routes: [{match: '', cache: {maxAge: '99999999999'}}]}}");
    if (host) {
        teqi(host->maxHeader, 10 * 1024, "Negative header limit uses its default");
        teqi(host->maxBody, 100 * 1024, "Negative body limit uses its default");
        teqi(host->maxBuffer, 64 * 1024, "Negative buffer limit uses its default");
        teqi((int) host->webSocketsMaxFrame, 100 * 1024, "Negative frame limit uses its default");
        teqi((int) host->webSocketsMaxMessage, 100 * 1024, "Negative message limit uses its default");
        route = routeAt(host, 0);
        teqi(route->cacheMaxAge, MAXINT, "An over-large cache max-age is clamped");
        webFreeHost(host);
    }
}

/*
    With no "web.routes" the host gets a single catch-all file route.
 */
static void testDefaultRoute(void)
{
    WebHost  *host;
    WebRoute *route;

    host = hostFromConfig("{web: {documents: './site'}}");
    if (!host) {
        return;
    }
    teqi(rGetListLength(host->routes), 1);
    route = routeAt(host, 0);
    tmatch(route->match, "");
    tmatch(route->handler, "file");
    //  No "validate" key means "validate whatever the signature file describes"
    teqi(route->validate, WEB_VALIDATE_DEFAULT);
    webFreeHost(host);
}

/*
    A trailing "/" makes a route a prefix match; anything else is an exact match. An empty
    pattern and a bare "/" are the two special cases.
 */
static void testRouteMatching(void)
{
    WebHost *host;

    host = hostFromConfig("{web: {documents: './site', routes: ["
                          "{match: '/api/', handler: 'action'},"
                          "{match: '/exact', handler: 'file'},"
                          "{match: '/', handler: 'file'},"
                          "{match: '', handler: 'file'}"
                          "]}}");
    if (!host) {
        return;
    }
    teqi(rGetListLength(host->routes), 4);

    //  Trailing slash - prefix match
    tfalse(routeAt(host, 0)->exact);
    tmatch(routeAt(host, 0)->handler, "action");

    //  No trailing slash - exact match
    ttrue(routeAt(host, 1)->exact);

    //  A bare "/" is an exact match
    ttrue(routeAt(host, 2)->exact);

    //  An empty pattern matches everything
    tfalse(routeAt(host, 3)->exact);
    webFreeHost(host);
}

/*
    Route flags and the per-route method set.
 */
static void testRouteFlags(void)
{
    WebHost  *host;
    WebRoute *route;

    host = hostFromConfig("{web: {documents: './site', routes: ["
                          "{match: '/a/', handler: 'action', validate: true, xsrf: true,"
                          " compressed: true, stream: true, role: 'admin', trim: '/a'},"
                          "{match: '/b/', methods: ['GET', 'POST']}"
                          "]}}");
    if (!host) {
        return;
    }
    route = routeAt(host, 0);
    teqi(route->validate, WEB_VALIDATE_ON);
    ttrue(route->xsrf);
    ttrue(route->compressed);
    ttrue(route->stream);
    tmatch(route->role, "admin");
    tmatch(route->trim, "/a");

    //  A route without a methods list shares the host default set
    ttrue(routeAt(host, 1)->methods != host->methods);
    tnotnull(rLookupName(routeAt(host, 1)->methods, "GET"));
    tnotnull(rLookupName(routeAt(host, 1)->methods, "POST"));
    tnull(rLookupName(routeAt(host, 1)->methods, "DELETE"));
    ttrue(routeAt(host, 0)->methods == host->methods);
    webFreeHost(host);
}

/*
    A mistyped route key is a security trap - a misspelled 'role' silently leaves a route
    unauthenticated - so it must be reported and ignored rather than accepted.
 */
static void testUnknownRouteKey(void)
{
    WebHost *host;

    host = hostFromConfig("{web: {documents: './site', routes: ["
                          "{match: '/a/', handler: 'file', rle: 'admin', authTyp: 'digest'}"
                          "]}}");
    if (!host) {
        return;
    }
    //  The route is still created, but the misspelled keys are not honoured
    teqi(rGetListLength(host->routes), 1);
    tnull(routeAt(host, 0)->role);
    tnull(routeAt(host, 0)->authType);
    webFreeHost(host);
}

/*
    Only MD5 and SHA-256 are supported digest algorithms. Anything else is discarded so the
    route falls back to the host default rather than silently using an unsupported algorithm.
 */
static void testBadDigestAlgorithm(void)
{
    WebHost *host;

    host = hostFromConfig("{web: {documents: './site', routes: ["
                          "{match: '/a/', authType: 'digest', algorithm: 'SHA-1'},"
                          "{match: '/b/', authType: 'digest', algorithm: 'MD5'},"
                          "{match: '/c/', authType: 'digest', algorithm: 'SHA-256'}"
                          "]}}");
    if (!host) {
        return;
    }
    tnull(routeAt(host, 0)->algorithm);
    tmatch(routeAt(host, 1)->algorithm, "MD5");
    tmatch(routeAt(host, 2)->algorithm, "SHA-256");
    webFreeHost(host);
}

/*
    The "web.redirect" collection maps a request path to a target and status.
 */
static void testRedirects(void)
{
    WebHost     *host;
    WebRedirect *redirect;

    //  No redirect collection leaves the list unallocated
    host = hostFromConfig("{web: {documents: './site'}}");
    if (host) {
        tnull(host->redirects);
        webFreeHost(host);
    }

    host = hostFromConfig("{web: {documents: './site', redirect: ["
                          "{from: '/old', to: '/new', status: 302},"
                          "{to: '/secure'},"
                          "{from: '/broken'}"
                          "]}}");
    if (!host) {
        return;
    }
    tnotnull(host->redirects);

    //  The third entry has no target and is skipped; the other two are kept in order
    teqi(rGetListLength(host->redirects), 2);

    redirect = (WebRedirect*) rGetItem(host->redirects, 0);
    tmatch(redirect->from, "/old");
    tmatch(redirect->to, "/new");
    teqi(redirect->status, 302);

    //  A missing "from" is allowed and the status defaults to 301
    redirect = (WebRedirect*) rGetItem(host->redirects, 1);
    tnull(redirect->from);
    tmatch(redirect->to, "/secure");
    teqi(redirect->status, 301);
    webFreeHost(host);
}

/*
    The canonical host name from web.name wins over the listening endpoint.
 */
static void testHostName(void)
{
    WebHost *host;
    Web     *web;
    char    *name;

    host = hostFromConfig("{web: {name: 'example.com', documents: './site'}}");
    if (!host) {
        return;
    }
    tmatch(host->name, "example.com");

    web = rAllocType(Web);
    web->host = host;
    name = webGetHostName(web);
    tmatch(name, "example.com");
    rFree(name);
    rFree(web);
    webFreeHost(host);
}

/*
    WEB_SHOW selects request and response tracing without a configuration change.
 */
static void testShowFlags(void)
{
    WebHost *host;

    putenv((char*) "WEB_SHOW=HBhb");
    host = hostFromConfig("{web: {documents: './site'}}");
    if (host) {
        ttrue(host->flags & WEB_SHOW_REQ_HEADERS);
        ttrue(host->flags & WEB_SHOW_REQ_BODY);
        ttrue(host->flags & WEB_SHOW_RESP_HEADERS);
        ttrue(host->flags & WEB_SHOW_RESP_BODY);
        webFreeHost(host);
    }

    //  A subset selects only those streams
    putenv((char*) "WEB_SHOW=H");
    host = hostFromConfig("{web: {documents: './site'}}");
    if (host) {
        ttrue(host->flags & WEB_SHOW_REQ_HEADERS);
        tfalse(host->flags & WEB_SHOW_RESP_BODY);
        webFreeHost(host);
    }

    //  An explicit flags argument suppresses the environment override
    putenv((char*) "WEB_SHOW=HBhb");
    {
        Json *config = jsonParse("{web: {documents: './site'}}", 0);
        host = webAllocHost(config, WEB_SHOW_RESP_BODY);
        tnotnull(host);
        if (host) {
            tfalse(host->flags & WEB_SHOW_REQ_HEADERS);
            ttrue(host->flags & WEB_SHOW_RESP_BODY);
            webFreeHost(host);
        }
    }
    putenv((char*) "WEB_SHOW=");
}

/*
    A null config makes the host read ME_WEB_CONFIG from the current directory and own the result.
 */
static void testDefaultConfigFile(void)
{
    WebHost *host;

    host = webAllocHost(NULL, 0);
    tnotnull(host);
    if (host) {
        //  test/web.json5 was loaded
        tnotnull(host->config);
        teqi(host->freeConfig, 1);
        ttrue(rGetListLength(host->routes) > 1);
        webFreeHost(host);
    }
}

static int hookProc(struct Web *web, int event)
{
    return 0;
}

static void testHook(void)
{
    WebHost *host;

    host = hostFromConfig("{web: {documents: './site'}}");
    if (!host) {
        return;
    }
    tnull(host->hook);
    webSetHook(host, hookProc);
    ttrue(host->hook == hookProc);
    webFreeHost(host);
}


static void testRouteMethods(void)
{
    WebHost  *host;
    WebRoute *route;

    host = hostFromConfig("{web: {documents: './site', routes: ["
                          "{match: '/a/', methods: ['GET', 'PUT']}, "
                          "{match: '/b/', methods: 'GET'}, "
                          "{match: '/c/', methods: ''}]}}");
    if (!host) {
        return;
    }
    route = routeAt(host, 0);
    ttrue(rLookupName(route->methods, "GET") && rLookupName(route->methods, "PUT"), "Array of methods");
    ttrue(rLookupName(route->methods, "HEAD"), "GET implies HEAD");

    route = routeAt(host, 1);
    ttrue(rLookupName(route->methods, "GET"), "A plain string names its method without trimming");

    route = routeAt(host, 2);
    teqi(rGetHashLength(route->methods), 0, "An empty methods string grants no methods");
    webFreeHost(host);
}

static void fiberMain(void *arg)
{
    webInit();
    testTimeoutBounds();
    testLimitBounds();
    testAllocFailureFreesHost();
    testDefaultRoute();
    testRouteMatching();
    testRouteFlags();
    testUnknownRouteKey();
    testBadDigestAlgorithm();
    testRedirects();
    testHostName();
    testShowFlags();
    testDefaultConfigFile();
    testHook();
    testRouteMethods();
    webTerm();
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
