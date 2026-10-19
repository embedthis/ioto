/*
    endpoint-forms.tst.c - Listen endpoint forms and the host name derived from them

    The agent listens on "http://:80" and "https://:443": endpoints that name a port but no host.
    The host name the server reports for such an endpoint feeds redirect targets and the
    Access-Control-Allow-Origin header, so it must be a usable "host:port", never ":port".
    Malformed ports must stop the host from starting rather than listen somewhere unintended.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

/*
    Start a host on the endpoint with a route that redirects to a relative target, request it,
    and return the Location the server built from its own host name.
 */
static bool locationFor(cchar *endpoint, int port, char *location, size_t size)
{
    WebHost *host;
    Url     *up;
    cchar   *value;
    char    config[512], url[128];
    int     status;

    SFMT(config, "{ web: { documents: './site', listen: ['%s'], "
         "routes: [ { match: '/r/', redirect: '/index.html' }, {} ] } }", endpoint);
    if ((host = startTestHost(config)) == 0) {
        tfail("Cannot start host on %s", endpoint);
        return 0;
    }
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "http://127.0.0.1:%d/r/", port), NULL, 0, NULL);
    teqi(status, 302, "%s", endpoint);
    value = urlGetHeader(up, "Location");
    scopy(location, size, value ? value : "");
    urlFree(up);
    stopTestHost(host);
    return status == 302;
}

/*
    Endpoints that name no host must still produce a usable host name
 */
static void testEndpointWithoutHostname(void)
{
    char location[128];

    if (locationFor("http://:4850", 4850, location, sizeof(location))) {
        tmatch(location, "http://localhost:4850/index.html");
    }
    if (locationFor(":4851", 4851, location, sizeof(location))) {
        tmatch(location, "http://localhost:4851/index.html");
    }
    if (locationFor("http://localhost:4852", 4852, location, sizeof(location))) {
        tmatch(location, "http://localhost:4852/index.html");
    }
}

/*
    Malformed ports must refuse to start, not listen on a default
 */
static void testBadPortsRefuseToStart(void)
{
    WebHost *host;
    Json    *json;
    //  4294972149 is 2^32 + 4853: truncated to int it is a valid, free port
    cchar   *bad[] = { "http://localhost:abc", "http://localhost:70000", "http://localhost:0", "http://localhost:-1",
                       "http://localhost:4294972149", "http://localhost:+4854", "http://localhost: 4855" };
    char    config[256];
    int     i;

    for (i = 0; i < (int) (sizeof(bad) / sizeof(bad[0])); i++) {
        SFMT(config, "{ web: { documents: './site', listen: ['%s'] } }", bad[i]);
        json = jsonParse(config, 0);
        host = webAllocHost(json, 0);
        tnotnull(host);
        if (host) {
            ttrue(webStartHost(host) < 0, "%s must not start", bad[i]);
            webStopHost(host);
            webFreeHost(host);
        }
        jsonFree(json);
    }
}

/*
    Stopping and restarting a host replaces its listeners rather than accumulating closed ones
 */
static void testRestartReplacesListeners(void)
{
    WebHost *host;
    Json    *json;

    json = jsonParse("{ web: { documents: './site', listen: ['http://127.0.0.1:4857'] } }", 0);
    host = webAllocHost(json, 0);
    tnotnull(host);
    if (host) {
        teqi(webStartHost(host), 0);
        teqi(rGetListLength(host->listeners), 1);
        webStopHost(host);
        teqi(webStartHost(host), 0, "The host restarts on the same endpoint");
        teqi(rGetListLength(host->listeners), 1, "A restart must not keep the closed listener");
        webStopHost(host);
        webFreeHost(host);
    }
    jsonFree(json);
}

/*
    A host that fails to start part way through its listen list must not leave the earlier listeners open
 */
static void testFailedStartClosesListeners(void)
{
    WebHost *host;
    Json    *json;
    Url     *up;

    json = jsonParse("{ web: { documents: './site', listen: ['http://127.0.0.1:4840', 'http://localhost:abc'] } }", 0);
    host = webAllocHost(json, 0);
    tnotnull(host);
    if (host) {
        ttrue(webStartHost(host) < 0, "The bad second endpoint must fail the start");
        up = urlAlloc(0);
        ttrue(urlFetch(up, "GET", "http://127.0.0.1:4840/index.html", NULL, 0, NULL) < 0,
              "The first endpoint must not be left listening");
        urlFree(up);
        webStopHost(host);
        webFreeHost(host);
    }
    jsonFree(json);
}

static void refuseConnection(cvoid *arg, RSocket *sp)
{
    rFreeSocket(sp);
}

/*
    A failed start keeps no listener, so a restart once the endpoint is free binds exactly the configured set
 */
static void testFailedStartKeepsNoListeners(void)
{
    WebHost *host;
    RSocket *busy;
    Json    *json;

    busy = rAllocSocket();
    teqi(rListenSocket(busy, "127.0.0.1", 4954, refuseConnection, NULL), 0);

    json = jsonParse("{ web: { documents: './site', "
                     "listen: ['http://127.0.0.1:4953', 'http://127.0.0.1:4954'] } }", 0);
    host = webAllocHost(json, 0);
    tnotnull(host);
    if (host) {
        ttrue(webStartHost(host) < 0, "A start with an endpoint in use must fail");
        teqi(rGetListLength(host->listeners), 0, "A failed start must not keep the endpoints that bound");
        rFreeSocket(busy);
        busy = 0;
        teqi(webStartHost(host), 0, "The host starts once the endpoint is free");
        teqi(rGetListLength(host->listeners), 2);
        webStopHost(host);
        webFreeHost(host);
    }
    rFreeSocket(busy);
    jsonFree(json);
}

#if ME_WEB_SESSIONS
/*
    Session pruning runs while the host is started, including after a restart
 */
static void testRestartPrunesSessions(void)
{
    WebHost *host;
    Json    *json;

    json = jsonParse("{ web: { documents: './site', listen: ['http://127.0.0.1:4841'] } }", 0);
    host = webAllocHost(json, 0);
    tnotnull(host);
    if (host) {
        teqi(webStartHost(host), 0);
        ttrue(rLookupEvent(host->sessionEvent), "Started host must prune sessions");
        webStopHost(host);
        tfalse(rLookupEvent(host->sessionEvent), "Stopped host must not prune sessions");
        teqi(webStartHost(host), 0);
        ttrue(rLookupEvent(host->sessionEvent), "Restarted host must prune sessions");
        webStopHost(host);
        webFreeHost(host);
    }
    jsonFree(json);
}
#endif

static void fiberMain(void *arg)
{
    if (setup(NULL, NULL)) {
        testEndpointWithoutHostname();
        testRestartReplacesListeners();
        testBadPortsRefuseToStart();
        testFailedStartClosesListeners();
        testFailedStartKeepsNoListeners();
#if ME_WEB_SESSIONS
        testRestartPrunesSessions();
#endif
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
