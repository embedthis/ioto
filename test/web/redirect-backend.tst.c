/*
    redirect-backend.tst.c - Redirect targets must not name the backend's address or leave the route

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define ENDPOINT "http://127.0.0.1:4877"

#define CONFIG                                                              \
    "{web: {documents: './site', name: 'device.example:8443',"              \
    " listen: ['" ENDPOINT "'],"                                            \
    " redirect: [{from: '/old', to: '/new', status: 302}],"                 \
    " routes: [{match: '/pubf/', trim: '/pubf', handler: 'file'}, {}]}}"

/************************************ Code ************************************/

/*
    Request a target and copy its Location into loc. Returns the status.
 */
static int location(cchar *target, char *loc, size_t locSize)
{
    cchar *segments[1], *cp, *end;
    char  *req, buf[8192];
    int   status;

    req = sfmt("GET %s HTTP/1.1\r\nHost: 127.0.0.1:4877\r\nConnection: close\r\n\r\n", target);
    segments[0] = req;
    status = rawRequest(ENDPOINT, segments, 1, 0, 1, buf, sizeof(buf));
    rFree(req);
    loc[0] = '\0';
    if ((cp = sncaselesscontains(buf, "\r\nLocation: ", slen(buf))) != 0) {
        cp += 12;
        if ((end = scontains(cp, "\r\n")) != 0) {
            sncopy(loc, locSize, cp, (size_t) (end - cp));
        }
    }
    return status;
}

static void testRedirectsIndependentOfBackend(void)
{
    WebHost *host;
    char    loc[1024];

    if ((host = startTestHost(CONFIG)) == 0) {
        return;
    }
    //  The directory redirect is relative, so it carries no scheme, host or port at all
    teqi(location("/dir", loc, sizeof(loc)), 301);
    tmatch(loc, "/dir/", "The directory redirect must be a relative reference");

    teqi(location("/dir?a=1", loc, sizeof(loc)), 301);
    tmatch(loc, "/dir/?a=1", "The directory redirect must keep the query");

    //  A trimmed request is redirected within the route the client asked for
    teqi(location("/pubf/dir", loc, sizeof(loc)), 301);
    tmatch(loc, "/pubf/dir/", "The Location must keep the trimmed route prefix");

    //  With a canonical name, a configured redirect uses that name's port, not the socket's
    teqi(location("/old", loc, sizeof(loc)), 302);
    tfalse(scontains(loc, ":4877") != 0, "The backend port must not appear in the Location");
    ttrue(sstarts(loc, "http://device.example:8443/new"), "The Location must use web.name, got %s", loc);
    stopTestHost(host);
}

static void fiberMain(void *arg)
{
    if (setup(NULL, NULL)) {
        testRedirectsIndependentOfBackend();
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
