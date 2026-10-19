/*
    session-api.tst.c - Session and cookie API invariants

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define CONFIG                                                                      \
    "{web: {documents: './site', listen: [],"                                       \
    " limits: {sessions: 10}, timeouts: {session: '30mins'},"                       \
    " sessions: {secure: 'always'},"                                                \
    " auth: {roles: {public: [], user: ['view'], admin: ['user', 'edit']}},"        \
    " routes: [{}]}}"

/************************************ Code ************************************/

static Web *allocTestWeb(WebHost *host, cchar *sessionId)
{
    Web *web;

    web = rAllocType(Web);
    web->host = host;
    web->txHeaders = rAllocHash(16, R_DYNAMIC_VALUE);
    web->signature = -1;
    web->status = 200;
    web->cookie = sessionId ? sfmt("%s=%s", host->sessionCookie, sessionId) : 0;
    return web;
}

static void freeTestWeb(Web *web)
{
    rFreeHash(web->txHeaders);
    rFree(web->cookie);
    rFree(web->sessionId);
    rFree(web->securityToken);
    rFree(web->username);
    rFree(web->role);
    rFree(web);
}


static char *login(WebHost *host, cchar *username, cchar *role)
{
    Web  *web;
    char *id;

    web = allocTestWeb(host, NULL);
    id = webLogin(web, username, role) ? sclone(web->sessionId) : 0;
    freeTestWeb(web);
    return id;
}

/*
    Invariant: no argument to webSetCookie can add a header or end the header block.
 */
static void testCookiePathInjection(WebHost *host)
{
    Web *web;
    int rc;

    web = allocTestWeb(host, NULL);
    rc = webSetCookie(web, "a", "b", "/p\r\nX-Injected: 1", 0, 0);
    teqi(rc, R_ERR_BAD_ARGS);
    rc = webSetCookie(web, "a", "b", "/p;Domain=evil", 0, 0);
    teqi(rc, R_ERR_BAD_ARGS);
    tnull(rLookupName(web->txHeaders, "Set-Cookie"));
    rc = webSetCookie(web, "a", "b", "/app/path", 0, 0);
    teqi(rc, 0);
    tcontains(rLookupName(web->txHeaders, "Set-Cookie"), "path=/app/path");
    freeTestWeb(web);
}

/*
    Invariant: a session variable named by a request-scoped string survives that string.
 */
static void testSessionVarKeyOwnership(WebHost *host)
{
    Web  *web;
    char *id, *key;

    id = login(host, "alice", "user");
    tnotnull(id);

    web = allocTestWeb(host, id);
    key = sclone("request-scoped-key");
    tnotnull(webSetSessionVar(web, key, "%s", "kept"));
    memset(key, 'X', slen(key));
    rFree(key);
    freeTestWeb(web);

    web = allocTestWeb(host, id);
    tmatch(webGetSessionVar(web, "request-scoped-key", 0), "kept");
    freeTestWeb(web);
    rFree(id);
}


static void testLoginRoleHonoured(WebHost *host)
{
    Web  *web;
    char *id, *oldId;
    bool ok;

    oldId = login(host, "carl", "admin");
    tnotnull(oldId);

    //  carl has since been demoted to "user" in the application's user store
    id = login(host, "carl", "user");
    tnotnull(id);
    web = allocTestWeb(host, id);
    ok = webAuthenticate(web);
    ttrue(ok);
    tmatch(web->role, "user");
    ok = webCan(web, "edit");
    tfalse(ok);
    freeTestWeb(web);
    rFree(id);

    //  The session opened under the old role no longer authenticates
    web = allocTestWeb(host, oldId);
    ok = webAuthenticate(web);
    tfalse(ok);
    freeTestWeb(web);
    rFree(oldId);

    //  dave has since been promoted to "admin": the login must succeed
    rFree(login(host, "dave", "user"));
    id = login(host, "dave", "admin");
    tnotnull(id);
    rFree(id);
}

/*
    Removing a user ends its sessions, so a later user of the same name does not inherit them. A password
    change ends the sessions opened with the old password.
 */
static void testRemovalRevokesSessions(WebHost *host)
{
    Web  *web;
    char *id, *reused;
    bool ok;

    id = login(host, "carol", "user");
    tnotnull(id);
    ttrue(webRemoveUser(host, "carol"));

    //  A new account reuses the name with the same role
    reused = login(host, "carol", "user");
    tnotnull(reused);

    web = allocTestWeb(host, id);
    ok = webAuthenticate(web);
    tfalse(ok, "A removed user's session must not authenticate a new user of the same name");
    freeTestWeb(web);

    web = allocTestWeb(host, reused);
    ok = webAuthenticate(web);
    ttrue(ok, "The new account's own session authenticates");
    freeTestWeb(web);
    rFree(id);
    rFree(reused);

    tnotnull(webAddUser(host, "erin", "SHA256:0000000000000000000000000000000000000000000000000000000000000001",
                        "user"));
    id = login(host, "erin", "user");
    tnotnull(id);
    ttrue(webUpdateUser(host, "erin",
                        "SHA256:0000000000000000000000000000000000000000000000000000000000000002", NULL));
    web = allocTestWeb(host, id);
    ok = webAuthenticate(web);
    tfalse(ok, "A password change must end the sessions opened with the old password");
    freeTestWeb(web);
    rFree(id);
}

static void fiberMain(void *arg)
{
    WebHost *host;
    Json    *config;

    webInit();
    config = jsonParse(CONFIG, 0);
    tnotnull(config);
    if (config && (host = webAllocHost(config, 0)) != 0) {
        testCookiePathInjection(host);
        testSessionVarKeyOwnership(host);
        testLoginRoleHonoured(host);
        testRemovalRevokesSessions(host);
        webFreeHost(host);
    }
    jsonFree(config);
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
