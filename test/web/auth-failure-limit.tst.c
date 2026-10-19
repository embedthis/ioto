/*
    auth-failure-limit.tst.c - Repeated authentication failures from one client are refused with 429

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define ENDPOINT "http://127.0.0.1:4952"

static RBuf *captured;

/************************************ Code ************************************/

static void captureLog(cchar *type, cchar *source, cchar *msg)
{
    rPutToBuf(captured, "%s: %s: %s\n", type, source, msg);
}

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

static void testFailureLimit(void)
{
    WebHost     *host;
    RLogHandler prior;
    cchar       *log;
    int         i, status;

    host = startTestHost("{ web: { documents: './site', listen: ['" ENDPOINT "'], "
                         "auth: { realm: 'Test Realm', authType: 'basic', requireTlsForBasic: false, "
                         "maxFailures: 3, failurePeriod: '2secs', "
                         "roles: { user: [], admin: ['user'] }, users: { "
                         "alice: { password: 'SHA256:11c66702489123a02c7dd0860e47fc4989bb0805ba5835fbbb36f316ec83eb83', "
                         "role: 'admin' } } }, "
                         "routes: [ { match: '/admin/', role: 'admin', authType: 'basic' }, {} ] } }");
    if (!host) {
        return;
    }
    captured = rAllocBuf(1024);
    prior = rSetLogHandler(captureLog);

    //  A challenge without credentials is not a failure
    for (i = 0; i < 5; i++) {
        status = fetch("/admin/secret.html", NULL);
        teqi(status, 401);
    }
    status = fetch("/admin/secret.html", "alice:password");
    teqi(status, 200, "Requests without credentials must not count toward the limit");

    for (i = 0; i < 3; i++) {
        status = fetch("/admin/secret.html", "alice:wrong-secret");
        teqi(status, 401);
    }
    rFlushBuf(captured);
    status = fetch("/admin/secret.html", "alice:password");
    teqi(status, 429, "The correct password is refused once the limit is reached");
    status = fetch("/admin/secret.html", "nosuchuser:password");
    teqi(status, 429);

    log = rBufToString(captured);
    tcontains(log, "too many failures");
    tcontains(log, "from 127.0.0.1");
    tfalse(scontains(log, "alice"), "The username must not be logged");

    //  Unauthenticated content is still served while the client is refused
    status = fetch("/index.html", NULL);
    teqi(status, 200);

    rSleep(2100);
    status = fetch("/admin/secret.html", "alice:password");
    teqi(status, 200, "The client is accepted again after the failure period");

    rSetLogHandler(prior);
    rFreeBuf(captured);
    captured = 0;
    stopTestHost(host);
}

/*
    Unknown users count as failures too, and the limit can be disabled.
 */
static void testUnknownUserAndDisabled(void)
{
    WebHost *host;
    int     i, status;

    host = startTestHost("{ web: { documents: './site', listen: ['" ENDPOINT "'], "
                         "auth: { realm: 'Test Realm', authType: 'basic', requireTlsForBasic: false, "
                         "maxFailures: 0, roles: { user: [], admin: ['user'] }, users: { "
                         "alice: { password: 'SHA256:11c66702489123a02c7dd0860e47fc4989bb0805ba5835fbbb36f316ec83eb83', "
                         "role: 'admin' } } }, "
                         "routes: [ { match: '/admin/', role: 'admin', authType: 'basic' }, {} ] } }");
    if (!host) {
        return;
    }
    for (i = 0; i < 15; i++) {
        status = fetch("/admin/secret.html", "nosuchuser:wrong");
        teqi(status, 401);
    }
    status = fetch("/admin/secret.html", "alice:password");
    teqi(status, 200, "maxFailures 0 disables the limit");
    stopTestHost(host);

    host = startTestHost("{ web: { documents: './site', listen: ['" ENDPOINT "'], "
                         "auth: { realm: 'Test Realm', authType: 'basic', requireTlsForBasic: false, "
                         "maxFailures: 2, roles: { user: [], admin: ['user'] }, users: { "
                         "alice: { password: 'SHA256:11c66702489123a02c7dd0860e47fc4989bb0805ba5835fbbb36f316ec83eb83', "
                         "role: 'admin' } } }, "
                         "routes: [ { match: '/admin/', role: 'admin', authType: 'basic' }, {} ] } }");
    if (!host) {
        return;
    }
    for (i = 0; i < 2; i++) {
        status = fetch("/admin/secret.html", "nosuchuser:wrong");
        teqi(status, 401);
    }
    status = fetch("/admin/secret.html", "alice:password");
    teqi(status, 429, "Failures for unknown users count toward the limit");
    stopTestHost(host);
}

static void fiberMain(void *data)
{
    testFailureLimit();
    testUnknownUserAndDisabled();
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
