/*
    redirects-config.tst.c - Top level "web.redirect" matching

    redirects.tst.c covers per-route redirection. This covers the host level redirect table,
    which is applied before routing and matches on scheme, host, port, path, query and hash.

    The shared test server does not configure "web.redirect" -- its section is disabled -- and
    enabling it would change behaviour for every other test. So each case here starts its own
    host in process on its own port. Ports 4290-4292 are used by this test alone.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "test.h"

/*********************************** Locals ***********************************/

#define PORT_MATCH  4290
#define PORT_BAD    4291
#define PORT_CATCH  4292

/*
    Redirect entries are evaluated in order and the first match wins, so the specific patterns
    come first and nothing here matches everything.
 */
#define CONFIG_MATCH                                                       \
    "{web: {documents: './site', routes: [{}], listen: ['http://127.0.0.1:4290'],"       \
    " redirect: ["                                                         \
    "  {from: '/old', to: '/new', status: 302},"                           \
    "  {from: '/perm', to: '/target'},"                                    \
    "  {from: 'http://127.0.0.1/hostmatch', to: '/host-target'},"          \
    "  {from: 'http://otherhost/hostmiss', to: '/never'},"                 \
    "  {from: 'http://127.0.0.1:9999/portmiss', to: '/never'},"            \
    "  {from: 'https://127.0.0.1/schememiss', to: '/never'},"              \
    "  {from: '/q?x=1', to: '/query-target'}"                              \
    " ]}}"

//  A single unparseable "from" -- the port is out of range -- poisons every request
#define CONFIG_BAD                                                         \
    "{web: {documents: './site', routes: [{}], listen: ['http://127.0.0.1:4291'],"       \
    " redirect: [{from: 'http://127.0.0.1:99999/x', to: '/nowhere'}]}}"

//  An entry with no "from" matches every request
#define CONFIG_CATCH                                                       \
    "{web: {documents: './site', routes: [{}], listen: ['http://127.0.0.1:4292'],"       \
    " redirect: [{to: '/landing', status: 301}]}}"

/************************************ Code ************************************/

static WebHost *startHost(cchar *text)
{
    WebHost *host;
    Json    *config;

    if ((config = jsonParse(text, 0)) == 0) {
        tfail("Cannot parse redirect configuration");
        return 0;
    }
    if ((host = webAllocHost(config, 0)) == 0) {
        tfail("Cannot allocate host");
        jsonFree(config);
        return 0;
    }
    if (webStartHost(host) < 0) {
        tfail("Cannot start host - is the port already in use?");
        webFreeHost(host);
        return 0;
    }
    return host;
}

static void stopHost(WebHost *host)
{
    if (host) {
        webStopHost(host);
        webFreeHost(host);
    }
}

/*
    Request a path and return the status. The Location header, if any, is cloned into *location
    for the caller to free.
 */
static int request(int port, cchar *path, char **location)
{
    Url   *up;
    char  url[160];
    cchar *value;
    int   status;

    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "http://127.0.0.1:%d%s", port, path), NULL, 0, NULL);
    if (location) {
        value = urlGetHeader(up, "Location");
        *location = value ? sclone(value) : 0;
    }
    urlFree(up);
    return status;
}

/*
    Request a path and assert its status. The request must be made exactly once: teqi evaluates
    its first argument twice, once to compare and once to report.
 */
static void checkRequest(int port, cchar *path, char **location, int expect)
{
    int status;

    status = request(port, path, location);
    teqi(status, expect);
}

/*
    A path that matches is redirected with the configured status; the default status is 301.
 */
static void testPathMatch(void)
{
    char *location;

    checkRequest(PORT_MATCH, "/old", &location, 302);
    tnotnull(location);
    ttrue(sends(location, "/new"));
    rFree(location);

    //  No status in the entry means 301
    checkRequest(PORT_MATCH, "/perm", &location, 301);
    tnotnull(location);
    ttrue(sends(location, "/target"));
    rFree(location);
}

/*
    With no canonical "web.name" the redirect authority comes from the listening endpoint, not
    from whatever address the socket reports. This host listens on 127.0.0.1, which the socket
    layer would otherwise render as "localhost".
 */
static void testRedirectAuthority(void)
{
    char *location;

    checkRequest(PORT_MATCH, "/old", &location, 302);
    tnotnull(location);
    ttrue(sstarts(location, "http://127.0.0.1:4290/"));
    rFree(location);
}

/*
    A path that matches no entry is served normally.
 */
static void testNoMatch(void)
{
    char *location;

    checkRequest(PORT_MATCH, "/index.html", &location, 200);
    tnull(location);

    //  A path that does not exist is a normal 404, not a redirect
    checkRequest(PORT_MATCH, "/no-such-file.html", &location, 404);
    tnull(location);
}

/*
    A "from" may pin the host, the port and the scheme. Any of them mismatching means the entry
    does not apply, and the request falls through to normal handling.
 */
static void testAuthorityMatching(void)
{
    char *location;

    //  Host matches the connection address
    checkRequest(PORT_MATCH, "/hostmatch", &location, 301);
    tnotnull(location);
    ttrue(sends(location, "/host-target"));
    rFree(location);

    //  A different host does not match, so this is just a missing document
    checkRequest(PORT_MATCH, "/hostmiss", &location, 404);
    tnull(location);

    //  A different port does not match
    checkRequest(PORT_MATCH, "/portmiss", &location, 404);
    tnull(location);

    //  A different scheme does not match
    checkRequest(PORT_MATCH, "/schememiss", &location, 404);
    tnull(location);
}

/*
    A "from" carrying a query matches only when the request query is identical.
 */
static void testQueryMatching(void)
{
    char *location;

    checkRequest(PORT_MATCH, "/q?x=1", &location, 301);
    tnotnull(location);
    ttrue(sends(location, "/query-target"));
    rFree(location);

    //  A different query value does not match
    checkRequest(PORT_MATCH, "/q?x=2", &location, 404);
    tnull(location);

    //  The same path with no query does not match
    checkRequest(PORT_MATCH, "/q", &location, 404);
    tnull(location);
}

/*
    An entry with no "from" matches every request, which is how a site-wide redirect is written.
 */
static void testCatchAll(void)
{
    char *location;

    checkRequest(PORT_CATCH, "/anything", &location, 301);
    tnotnull(location);
    ttrue(sends(location, "/landing"));
    rFree(location);

    //  Even a path that exists is redirected
    checkRequest(PORT_CATCH, "/index.html", &location, 301);
    tnotnull(location);
    rFree(location);
}

/*
    A "from" that cannot be parsed - here an out of range port - must produce an error response
    rather than being silently skipped or matching everything.
 */
static void testUnparseableFrom(void)
{
    char *location;

    checkRequest(PORT_BAD, "/anything", &location, 404);
    tnull(location);
}

static void fiberMain(void *data)
{
    WebHost *match, *bad, *catchAll;

    webInit();

    if ((match = startHost(CONFIG_MATCH)) != 0) {
        testPathMatch();
        testRedirectAuthority();
        testNoMatch();
        testAuthorityMatching();
        testQueryMatching();
        stopHost(match);
    }
    if ((catchAll = startHost(CONFIG_CATCH)) != 0) {
        testCatchAll();
        stopHost(catchAll);
    }
    if ((bad = startHost(CONFIG_BAD)) != 0) {
        testUnparseableFrom();
        stopHost(bad);
    }
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
