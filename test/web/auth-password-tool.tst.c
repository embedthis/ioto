/*
    auth-password-tool.tst.c - A hash made the way the password utility makes it verifies through webCheckPassword

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static WebHost *allocHost(cchar *config)
{
    Json    *json;
    WebHost *host;

    if ((json = jsonParse(config, 0)) == 0) {
        tfail("Cannot parse host config");
        return 0;
    }
    if ((host = webAllocHost(json, 0)) == 0) {
        jsonFree(json);
        tfail("Cannot allocate host");
    }
    return host;
}

static void freeHost(WebHost *host)
{
    Json *json;

    if (host) {
        json = host->config;
        webFreeHost(host);
        jsonFree(json);
    }
}

/*
    The utility hashes "username:realm:password" with bcrypt at the default cost and defaults the realm to
    "web", the server's own default.
 */
static char *toolHash(cchar *username, cchar *realm, cchar *password)
{
    char *input, *hash;

    input = sfmt("%s:%s:%s", username, realm, password);
    hash = cryptMakePassword(input, CRYPT_BLOWFISH_SALT_LENGTH, CRYPT_BLOWFISH_ROUNDS);
    rFree(input);
    return hash;
}

static void testToolHashVerifies(void)
{
    WebHost *host, *other;
    char    *stored;

    host = allocHost("{ web: { documents: './site', auth: { roles: { user: [] } } } }");
    other = allocHost("{ web: { documents: './site', auth: { realm: 'other', roles: { user: [] } } } }");
    if (!host || !other) {
        freeHost(host);
        freeHost(other);
        return;
    }
    stored = toolHash("admin", "web", "Synthetic-Test-1");

    ttrue(webCheckPassword(host, "admin", "Synthetic-Test-1", stored), "The utility's hash must verify");
    tfalse(webCheckPassword(host, "admin", "wrong-password", stored), "A wrong password must fail");
    tfalse(webCheckPassword(host, "someone", "Synthetic-Test-1", stored), "The username is part of the hash");
    tfalse(webCheckPassword(other, "admin", "Synthetic-Test-1", stored), "A different realm must fail");
    tfalse(webCheckPassword(host, "admin", "Synthetic-Test-1", NULL), "An unknown user must fail");
    tfalse(webCheckPassword(host, "admin", NULL, stored));
    tfalse(webCheckPassword(NULL, "admin", "Synthetic-Test-1", stored));

    rFree(stored);
    freeHost(host);
    freeHost(other);
}

/*
    An unknown user is checked against a decoy with the default bcrypt cost, so a miss costs about the same
    as a wrong password for a known user.
 */
static void testUnknownUserCost(void)
{
    WebHost *host;
    Ticks   start, known, unknown;
    char    *stored;
    int     i;

    if ((host = allocHost("{ web: { documents: './site', auth: { roles: { user: [] } } } }")) == 0) {
        return;
    }
    stored = toolHash("admin", "web", "Synthetic-Test-1");

    start = rGetHiResTicks();
    for (i = 0; i < 5; i++) {
        webCheckPassword(host, "admin", "wrong-password", stored);
    }
    known = rGetHiResTicks() - start;

    start = rGetHiResTicks();
    for (i = 0; i < 5; i++) {
        webCheckPassword(host, "nosuchuser", "wrong-password", NULL);
    }
    unknown = rGetHiResTicks() - start;

    tinfo("known %lld, unknown %lld", (long long) known, (long long) unknown);
    ttrue(unknown * 2 >= known, "An unknown user must cost at least half of a known user's check");

    rFree(stored);
    freeHost(host);
}

static void fiberMain(void *data)
{
    testToolHashVerifies();
    testUnknownUserCost();
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
