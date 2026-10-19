/*
    auth-log.tst.c - Authentication refusals are logged

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define ENDPOINT "http://127.0.0.1:4899"

static RBuf *captured;

/************************************ Code ************************************/

static void captureLog(cchar *type, cchar *source, cchar *msg)
{
    rPutToBuf(captured, "%s: %s: %s\n", type, source, msg);
}

/*
    Fetch a page with the given Basic credentials and return the status
 */
static int fetch(cchar *path, cchar *credentials)
{
    Url  *up;
    char headers[256], url[256], *encoded;
    int  status;

    headers[0] = '\0';
    if (credentials) {
        encoded = cryptEncode64(credentials);
        SFMT(headers, "Authorization: Basic %s\r\n", encoded);
        rFree(encoded);
    }
    up = urlAlloc(0);
    status = urlFetch(up, "GET", sfmtbuf(url, sizeof(url), "%s%s", ENDPOINT, path), NULL, 0, headers);
    urlFree(up);
    return status;
}

static void testRefusalsLogged(void)
{
    WebHost     *host;
    RLogHandler prior;
    cchar       *log;
    int         status;

    host = startTestHost("{ web: { documents: './site', listen: ['" ENDPOINT "'], "
                         "auth: { realm: 'Test Realm', authType: 'basic', requireTlsForBasic: false, "
                         "roles: { user: [], admin: ['user'], super: ['admin'] }, users: { "
                         "alice: { password: 'SHA256:11c66702489123a02c7dd0860e47fc4989bb0805ba5835fbbb36f316ec83eb83', "
                         "role: 'admin' } } }, "
                         "routes: [ { match: '/admin/', role: 'admin', authType: 'basic' }, { match: '/basic/', role: 'super', authType: 'basic' }, {} ] } }");
    if (!host) {
        return;
    }
    captured = rAllocBuf(1024);
    prior = rSetLogHandler(captureLog);

    status = fetch("/admin/secret.html", NULL);
    teqi(status, 401);
    tfalse(scontains(rBufToString(captured), "Refused"), "A request without credentials is not logged");

    status = fetch("/admin/secret.html", "alice:wrong-secret");
    teqi(status, 401);
    log = rBufToString(captured);
    tcontains(log, "auth: Refused GET from 127.0.0.1");
    tcontains(log, "authentication failed");
    tfalse(scontains(log, "wrong-secret"), "The password must never be logged");
    tfalse(scontains(log, "alice"), "The client-supplied username must not be logged");

    rFlushBuf(captured);
    status = fetch("/basic/index.html", "alice:password");
    teqi(status, 403);
    tcontains(rBufToString(captured), "insufficient privilege");

    status = fetch("/admin/secret.html", "alice:password");
    teqi(status, 200, "The correct credentials must succeed or the refusals prove nothing");

    rSetLogHandler(prior);
    rFreeBuf(captured);
    captured = 0;
    stopTestHost(host);
}

static void fiberMain(void *data)
{
    testRefusalsLogged();
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
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
