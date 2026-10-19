/*
    auth-no-credential.tst.c - A user without a stored credential must not authenticate by HTTP auth

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define ENDPOINT "http://127.0.0.1:4880"

/************************************ Code ************************************/

/*
    Simulate application-managed login after its password check succeeds.
 */
static void dbLoginAction(Web *web)
{
    if (webLogin(web, webGetQueryVar(web, "username", ""), "admin")) {
        webWriteResponse(web, 200, "logged in");
    } else {
        webWriteResponse(web, 400, "login failed");
    }
}

static void secureAction(Web *web)
{
    webWriteResponse(web, 200, "SECRET");
}

static bool param(cchar *header, cchar *name, char *value, size_t size)
{
    cchar *cp, *end;
    char  key[64];

    SFMT(key, "%s=\"", name);
    if ((cp = scontains(header, key)) == 0) {
        return 0;
    }
    cp += slen(key);
    if ((end = schr(cp, '"')) == 0) {
        return 0;
    }
    sncopy(value, size, cp, (size_t) (end - cp));
    return 1;
}

static int sendOne(RSocket *sp, cchar *request, char *head, size_t headSize)
{
    char  buf[8192];
    ssize n;

    if (rWriteSocket(sp, request, slen(request), rGetTicks() + 5 * TPS) < 0) {
        return -1;
    }
    if ((n = rawRead(sp, buf, sizeof(buf), 1)) <= 12) {
        return -1;
    }
    if (head) {
        scopy(head, headSize, buf);
    }
    return (int) stoi(&buf[9]);
}

/*
    Answer a fresh Digest challenge for "username" with an empty HA1 - the credential nobody was
    ever issued.
 */
static int emptyHa1Request(cchar *username)
{
    RSocket *sp;
    char    head[8192], nonce[512], opaque[256], request[2048], *ha2, *preimage, *response;
    cchar   *uri;
    int     status;

    uri = "/secure/data";
    if ((sp = rawConnect(ENDPOINT)) == 0) {
        return -1;
    }
    if (sendOne(sp, "GET /secure/data HTTP/1.1\r\nHost: localhost\r\n\r\n", head, sizeof(head)) != 401 ||
        !param(head, "nonce", nonce, sizeof(nonce))) {
        rFreeSocket(sp);
        return -1;
    }
    if (!param(head, "opaque", opaque, sizeof(opaque))) {
        opaque[0] = '\0';
    }
    preimage = sfmt("GET:%s", uri);
    ha2 = cryptGetSha256((cuchar*) preimage, slen(preimage));
    rFree(preimage);
    preimage = sfmt(":%s:00000001:0a1b2c3d:auth:%s", nonce, ha2);
    response = cryptGetSha256((cuchar*) preimage, slen(preimage));
    rFree(preimage);
    SFMT(request, "GET %s HTTP/1.1\r\nHost: localhost\r\nAuthorization: Digest username=\"%s\", "
         "realm=\"Test Realm\", nonce=\"%s\", uri=\"%s\", qop=auth, nc=00000001, cnonce=\"0a1b2c3d\", "
         "response=\"%s\", algorithm=SHA-256%s%s%s\r\n\r\n",
         uri, username, nonce, uri, response, opaque[0] ? ", opaque=\"" : "", opaque, opaque[0] ? "\"" : "");
    status = sendOne(sp, request, NULL, 0);
    rFree(ha2);
    rFree(response);
    rFreeSocket(sp);
    return status;
}

static int fetch(cchar *url)
{
    Url *up;
    int status;

    up = urlAlloc(0);
    status = urlFetch(up, "GET", url, NULL, 0, NULL);
    urlFree(up);
    return status;
}

static void testEmptyHa1(void)
{
    WebHost *host;
    int     status;

    host = startTestHost("{ web: { documents: '.', listen: ['" ENDPOINT "'], "
                         "auth: { realm: 'Test Realm', authType: 'digest', algorithm: 'SHA-256', "
                         "secret: 'test-secret-1234567890abcdef', "
                         "roles: { public: [], user: [], admin: ['user'] } }, "
                         "routes: [ { match: '/secure/', authType: 'digest', role: 'admin', handler: 'action' }, "
                         "{ match: '/login/', handler: 'action' }, {} ] } }");
    if (!host) {
        return;
    }
    webAddAction(host, "/login/db", dbLoginAction, NULL);
    webAddAction(host, "/secure/data", secureAction, NULL);


    status = emptyHa1Request("carol");
    teqi(status, 401, "Unknown user is refused");

    //  carol signs in through the application, with her own password checked by the application
    status = fetch(ENDPOINT "/login/db?username=carol");
    teqi(status, 200, "Application login succeeds");

    //  Present an empty-HA1 Digest response for the named user.
    status = emptyHa1Request("carol");
    teqi(status, 401, "A webLogin-created user must not authenticate with an empty Digest HA1");

    stopTestHost(host);
}

static void testEmptyConfigPassword(void)
{
    WebHost *host;
    int     status;

    host = startTestHost("{ web: { documents: '.', listen: ['" ENDPOINT "'], "
                         "auth: { realm: 'Test Realm', authType: 'digest', algorithm: 'SHA-256', "
                         "secret: 'test-secret-1234567890abcdef', "
                         "roles: { public: [], user: [], admin: ['user'] }, "
                         "users: { eve: { password: '', role: 'admin' } } }, "
                         "routes: [ { match: '/secure/', authType: 'digest', role: 'admin', handler: 'action' }, {} ] } }");
    if (!host) {
        return;
    }
    webAddAction(host, "/secure/data", secureAction, NULL);
    status = emptyHa1Request("eve");
    teqi(status, 401, "A configured user with an empty password must not authenticate by Digest");
    stopTestHost(host);
}

static void fiberMain(void *arg)
{
    testEmptyHa1();
    testEmptyConfigPassword();
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
