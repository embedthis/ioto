/*
    file-substitute-auth.tst.c - A file the handler substitutes must pass the route that guards it

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define ENDPOINT "http://127.0.0.1:4894"

/************************************ Code ************************************/

static int get(cchar *target, cchar *extra, char *buf, size_t size)
{
    cchar *segments[1];
    char  *req;
    int   status;

    req = sfmt("GET %s HTTP/1.1\r\nHost: x\r\n%sConnection: close\r\n\r\n", target, extra ? extra : "");
    segments[0] = req;
    status = rawRequest(ENDPOINT, segments, 1, 0, 1, buf, size);
    rFree(req);
    return status;
}

static void testSubstitutedFileIsAuthorized(void)
{
    WebHost *host;
    char    buf[8192];
    int     status;

    host = startTestHost("{ web: { documents: './site', listen: ['" ENDPOINT "'], routes: [ "
                         "{ match: '/dir/index.html', role: 'admin', handler: 'file' }, "
                         "{ match: '/compressed/app.js.gz', role: 'admin', handler: 'file', compressed: true }, "
                         "{ match: '/compressed/', handler: 'file', compressed: true }, "
                         "{ match: '/pubf/', trim: '/pubf', handler: 'file' }, "
                         "{} ] } }");
    if (!host) {
        return;
    }
    //  Controls: the protected names themselves are refused
    status = get("/dir/index.html", 0, buf, sizeof(buf));
    teqi(status, 401, "The protected index is refused by name");
    status = get("/compressed/app.js.gz", 0, buf, sizeof(buf));
    teqi(status, 401, "The protected variant is refused by name");

    //  The directory form opens the same protected file
    status = get("/dir/", 0, buf, sizeof(buf));
    ttrue(status != 200, "The directory index must not bypass the route protecting it (status %d)", status);
    tfalse(scontains(buf, "dir/index") != 0, "The protected index body must not be served");

    //  Through a public trim route the same substitution applies
    status = get("/pubf/dir/", 0, buf, sizeof(buf));
    ttrue(status != 200, "A trim route must not reach the protected index (status %d)", status);

    //  Content negotiation selects the protected pre-compressed variant
    status = get("/compressed/app.js", "Accept-Encoding: gzip\r\n", buf, sizeof(buf));
    tfalse(status == 200 && sncaselesscontains(buf, "Content-Encoding: gzip", slen(buf)) != 0,
           "The protected gzip variant must not be served through its uncompressed name");

    //  Unprotected substitutions keep working
    status = get("/compressed/", "Accept-Encoding: gzip\r\n", buf, sizeof(buf));
    teqi(status, 200,
         "An unprotected index is still served");
    stopTestHost(host);
}


static void testDeniedIndex(void)
{
    WebHost *host;
    char    buf[8192];
    int     status;

    host = startTestHost("{ web: { documents: './site', listen: ['" ENDPOINT "'], deny: ['index.html'], "
                         "routes: [ { handler: 'file' } ] } }");
    if (!host) {
        return;
    }
    status = get("/dir/index.html", 0, buf, sizeof(buf));
    teqi(status, 404, "The denied index is refused by name");
    status = get("/dir/", 0, buf, sizeof(buf));
    teqi(status, 404, "The directory form must not serve a denied index");
    stopTestHost(host);
}

static void fiberMain(void *arg)
{
    testSubstitutedFileIsAuthorized();
    testDeniedIndex();
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
    This is proprietary software and requires a commercial license from the author.
 */
