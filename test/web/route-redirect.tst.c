/*
    route-redirect.tst.c - A route "redirect" is terminal and never replaces the role check

    A route redirect used to be evaluated as the alternative to the route's role check, and issuing
    it left the request in the normal dispatch flow. The response was safe - the second write was
    suppressed - but everything the handler did before writing still happened, so PUT and DELETE
    mutated the filesystem behind the 302, unauthenticated when the route also carried a role.

    Each case runs against an in-process host on port 4294, which this test uses alone, because the
    shared server configures no route redirects.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define ENDPOINT "http://127.0.0.1:4294"
#define VICTIM   "site/data/redirect-victim.txt"
#define CREATED  "site/data/redirect-created.txt"

//  SHA-256("alice:Test Realm:password"), as stored in the shared web.json5
#define ALICE    "SHA256:11c66702489123a02c7dd0860e47fc4989bb0805ba5835fbbb36f316ec83eb83"

#define CONFIG                                                              \
    "{web: {documents: './site', listen: ['http://127.0.0.1:4294'],"        \
    " auth: {realm: 'Test Realm',"                                          \
    "  roles: {public: [], user: ['view'], admin: ['user']},"               \
    "  users: {alice: {password: '" ALICE "', role: 'admin'}}},"            \
    " routes: ["                                                            \
    "  {match: '/data/', redirect: '/moved', methods: ['GET', 'PUT', 'DELETE']},"  \
    "  {match: '/guard/', role: 'admin', redirect: '/login', methods: ['GET', 'DELETE']},"  \
    "  {}"                                                                  \
    " ]}}"

/************************************ Code ************************************/

static int fetch(cchar *method, cchar *path, cchar *body, cchar **location)
{
    static char header[512];
    Url         *up;
    cchar       *value;
    char        url[256];
    int         status;

    up = urlAlloc(0);
    status = urlFetch(up, method, SFMT(url, "%s%s", ENDPOINT, path), body, body ? (size_t) -1 : 0, NULL);
    header[0] = '\0';
    if ((value = urlGetHeader(up, "Location")) != 0) {
        scopy(header, sizeof(header), value);
    }
    if (location) {
        *location = header;
    }
    urlFree(up);
    return status;
}

/*
    A redirect must terminate dispatch: the file handler must not run and must not mutate the
    document tree behind the 302.
 */
static void testRedirectIsTerminal(void)
{
    cchar *location;
    int   status;

    ttrue(rWriteFile(VICTIM, "victim\n", 7, 0600) > 0, "Cannot create the victim file");

    status = fetch("GET", "/data/redirect-victim.txt", NULL, &location);
    teqi(status, 302, "A route redirect must still redirect");
    tcontains(location, "/moved");

    status = fetch("DELETE", "/data/redirect-victim.txt", NULL, NULL);
    teqi(status, 302, "A DELETE on a redirect route must be redirected");
    ttrue(rFileExists(VICTIM), "A redirected DELETE must not remove the file");

    status = fetch("PUT", "/data/redirect-created.txt", "content", NULL);
    teqi(status, 302, "A PUT on a redirect route must be redirected");
    ttrue(!rFileExists(CREATED), "A redirected PUT must not create the file");

    unlink(VICTIM);
    unlink(CREATED);
}

/*
    A route carrying both a role and a redirect must authorize first. Redirecting an anonymous
    request tells the client the resource moved and silently skips the gate.
 */
static void testRedirectDoesNotSkipRole(void)
{
    int status;

    status = fetch("GET", "/guard/secret", NULL, NULL);
    teqi(status, 401, "A role-bearing redirect route must challenge, not redirect");
}

static void fiberMain(void *arg)
{
    WebHost *host;

    if ((host = startTestHost(CONFIG)) != 0) {
        testRedirectIsTerminal();
        testRedirectDoesNotSkipRole();
        stopTestHost(host);
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
