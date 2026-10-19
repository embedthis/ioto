/*
    auth-basic-tls.tst.c - requireTlsForBasic stops the Basic challenge, not only the credential

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define ENDPOINT "http://127.0.0.1:4888"
#define ALICE    "SHA256:11c66702489123a02c7dd0860e47fc4989bb0805ba5835fbbb36f316ec83eb83"

/************************************ Code ************************************/

static void checkChallenge(bool requireTls)
{
    WebHost *host;
    Url     *up;
    cchar   *challenge;
    char    config[1024];
    int     status;

    SFMT(config, "{ web: { documents: './site', listen: ['" ENDPOINT "'], "
         "auth: { realm: 'Test Realm', authType: 'basic', requireTlsForBasic: %s, "
         "roles: { public: [], user: [], admin: ['user'] }, "
         "users: { alice: { password: '" ALICE "', role: 'admin' } } }, "
         "routes: [ { match: '/basic/', authType: 'basic', role: 'user', handler: 'file' }, {} ] } }",
         requireTls ? "true" : "false");
    if ((host = startTestHost(config)) == 0) {
        return;
    }
    up = urlAlloc(0);
    status = urlFetch(up, "GET", ENDPOINT "/basic/secret.html", NULL, 0, NULL);
    challenge = urlGetHeader(up, "WWW-Authenticate");
    if (requireTls) {
        teqi(status, 403, "A Basic route on cleartext is refused when TLS is required");
        tfalse(challenge && sstarts(challenge, "Basic"), "No Basic challenge is issued on cleartext");
    } else {
        teqi(status, 401, "A Basic route is challenged when TLS is not required");
        ttrue(challenge && sstarts(challenge, "Basic"), "A Basic challenge is issued");
    }
    urlFree(up);
    stopTestHost(host);
}

static void fiberMain(void *arg)
{
    checkChallenge(1);
    checkChallenge(0);
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
