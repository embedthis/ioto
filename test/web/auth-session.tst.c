/*
    auth-session.tst.c - Unit tests for session-backed request authentication

    webAuthenticate resolves a request to a user from the session cookie alone. These exercise
    that path directly against a WebHost: login establishes the session, a second request
    presenting the same cookie is authenticated from it, and the session is re-checked against
    the live user table so a role change or a deleted user revokes access.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

static Web *allocTestWeb(WebHost *host, cchar *cookie)
{
    Web *web;

    web = rAllocType(Web);
    web->host = host;
    web->txHeaders = rAllocHash(16, R_DYNAMIC_VALUE);
    web->signature = -1;
    web->status = 200;
    web->cookie = cookie ? sclone(cookie) : 0;
    return web;
}

static void freeTestWeb(Web *web)
{
    rFreeHash(web->txHeaders);
    rFree(web->username);
    rFree(web->role);
    rFree(web->cookie);
    rFree(web->securityToken);
    rFree(web);
}

/*
    Build the Cookie header a client would send back for the session established on "web".
 */
static char *sessionCookie(Web *web)
{
    ttrue(web->session != NULL);
    if (!web->session) {
        return sclone("");
    }
    return sfmt("%s=%s", web->host->sessionCookie, web->session->id);
}

/*
    Logging in creates a session, records the identity in it and marks the request authenticated.
 */
static void testLogin(WebHost *host)
{
    Web   *web;
    cchar *setCookie;

    web = allocTestWeb(host, NULL);
    tfalse(web->authenticated);

    ttrue(webLogin(web, "alice", "admin"));
    ttrue(web->authenticated);
    tmatch(web->username, "alice");
    tmatch(web->role, "admin");
    tnotnull(web->session);
    ttrue(web->user == webLookupUser(host, "alice"));

    //  The session cookie is handed to the client
    setCookie = rLookupName(web->txHeaders, "Set-Cookie");
    tnotnull(setCookie);
    tcontains(setCookie, host->sessionCookie);

    //  The identity is recorded in the session, which is what the next request reads
    tmatch(webGetSessionVar(web, WEB_SESSION_USERNAME, 0), "alice");
    tmatch(webGetSessionVar(web, WEB_SESSION_ROLE, 0), "admin");

    webDestroySession(web);
    freeTestWeb(web);
}

/*
    A login is refused when the user does not hold the requested ability. 'bob' is a 'user' and
    cannot log in as 'admin'.
 */
static void testLoginRefused(WebHost *host)
{
    Web *web;

    web = allocTestWeb(host, NULL);
    tfalse(webLogin(web, "bob", "admin"));
    tfalse(web->authenticated);
    tnull(web->user);
    freeTestWeb(web);

    //  An ability rather than a role name is accepted if the user holds it
    web = allocTestWeb(host, NULL);
    ttrue(webLogin(web, "bob", "read"));
    ttrue(web->authenticated);
    //  The session records the user's own role, not the ability that was asked for
    tmatch(web->role, "user");
    webDestroySession(web);
    freeTestWeb(web);

    /*
        An unknown user is created on demand with the requested role - this is the hook for
        applications that keep their users in a database rather than in web.json5.
     */
    web = allocTestWeb(host, NULL);
    ttrue(webLogin(web, "newcomer", "user"));
    tnotnull(webLookupUser(host, "newcomer"));
    tmatch(webLookupUser(host, "newcomer")->role, "user");
    webDestroySession(web);
    freeTestWeb(web);

    //  But not with a role that does not exist
    web = allocTestWeb(host, NULL);
    tfalse(webLogin(web, "impostor", "no-such-role"));
    tnull(webLookupUser(host, "impostor"));
    freeTestWeb(web);
}

/*
    A later request carrying the session cookie is authenticated from the session with no
    credentials presented.
 */
static void testAuthenticateFromCookie(WebHost *host)
{
    Web  *login, *web;
    char *cookie;

    login = allocTestWeb(host, NULL);
    ttrue(webLogin(login, "alice", "admin"));
    cookie = sessionCookie(login);

    web = allocTestWeb(host, cookie);
    ttrue(webAuthenticate(web));
    ttrue(web->authenticated);
    tmatch(web->username, "alice");
    tmatch(web->role, "admin");
    ttrue(web->user == webLookupUser(host, "alice"));

    //  Authorization now follows from the session
    ttrue(webCan(web, "edit"));
    ttrue(webCan(web, "read"));
    tfalse(webCan(web, "billing"));
    tmatch(webGetRole(web), "admin");

    //  The result is cached - a second call does not redo the work
    ttrue(webAuthenticate(web));
    ttrue(webIsAuthenticated(web));
    freeTestWeb(web);

    //  webIsAuthenticated authenticates on demand when nothing has been checked yet
    web = allocTestWeb(host, cookie);
    tfalse(web->authChecked);
    ttrue(webIsAuthenticated(web));
    ttrue(web->authChecked);
    freeTestWeb(web);

    rFree(cookie);
    webDestroySession(login);
    freeTestWeb(login);
}

/*
    No cookie, an unknown session id, or a malformed cookie must all leave the request anonymous.
 */
static void testAuthenticateRejected(WebHost *host)
{
    Web  *web;
    char cookie[128];

    //  No cookie at all
    web = allocTestWeb(host, NULL);
    tfalse(webAuthenticate(web));
    tfalse(web->authenticated);
    tnull(web->user);
    freeTestWeb(web);

    //  A session id that was never issued
    web = allocTestWeb(host, SFMT(cookie, "%s=nosuchsessionid", host->sessionCookie));
    tfalse(webAuthenticate(web));
    tnull(web->user);
    freeTestWeb(web);

    //  A cookie for a different name
    web = allocTestWeb(host, "unrelated=value");
    tfalse(webAuthenticate(web));
    freeTestWeb(web);

    /*
        A session that exists but carries no username is anonymous - webAddSecurityToken creates
        exactly this kind of session to hand an XSRF token to a client that has not logged in.
     */
    web = allocTestWeb(host, NULL);
    webGetSecurityToken(web, 0);
    tnotnull(web->session);
    {
        char *anon = sessionCookie(web);
        Web  *other = allocTestWeb(host, anon);
        tfalse(webAuthenticate(other));
        tnull(other->user);
        freeTestWeb(other);
        rFree(anon);
    }
    webDestroySession(web);
    freeTestWeb(web);
}

/*
    The session stores the role it was issued with. On each request that role is re-checked
    against the live user record, so an out-of-band role change revokes the session rather than
    letting it keep the role it was granted.
 */
static void testRoleChangeRevokesSession(WebHost *host)
{
    Web  *login, *web;
    char *cookie;

    ttrue(webAddUser(host, "kim", "hash", "admin") != NULL);

    login = allocTestWeb(host, NULL);
    ttrue(webLogin(login, "kim", "admin"));
    cookie = sessionCookie(login);

    //  The session authenticates while the role still matches
    web = allocTestWeb(host, cookie);
    ttrue(webAuthenticate(web));
    freeTestWeb(web);

    //  Demote the user out of band
    ttrue(webUpdateUser(host, "kim", NULL, "user"));

    //  The session now names a role the user no longer holds, so it is refused
    web = allocTestWeb(host, cookie);
    tfalse(webAuthenticate(web));
    tfalse(web->authenticated);
    tnull(web->user);
    tnull(web->username, "A refused session must not publish its username");
    tnull(web->role, "A refused session must not publish its role");
    tfalse(webCan(web, "edit"));
    freeTestWeb(web);

    rFree(cookie);
    webDestroySession(login);
    freeTestWeb(login);
}

/*
    Deleting the user invalidates any session that names them.
 */
static void testDeletedUserRevokesSession(WebHost *host)
{
    Web  *login, *web;
    char *cookie;

    ttrue(webAddUser(host, "lee", "hash", "user") != NULL);

    login = allocTestWeb(host, NULL);
    ttrue(webLogin(login, "lee", "user"));
    cookie = sessionCookie(login);

    ttrue(webRemoveUser(host, "lee"));

    web = allocTestWeb(host, cookie);
    tfalse(webAuthenticate(web));
    tnull(web->user);
    tnull(web->username, "A session naming a removed user must not publish the username");
    tnull(web->role, "A session naming a removed user must not publish the role");
    freeTestWeb(web);

    rFree(cookie);
    webDestroySession(login);
    freeTestWeb(login);
}

/*
    Logging out clears the request identity and destroys the session, so the cookie the client
    still holds no longer authenticates.
 */
static void testLogout(WebHost *host)
{
    Web  *login, *web;
    char *cookie;

    login = allocTestWeb(host, NULL);
    ttrue(webLogin(login, "alice", "admin"));
    cookie = sessionCookie(login);

    webLogout(login);
    tfalse(login->authenticated);
    tnull(login->username);
    tnull(login->role);
    tnull(login->user);
    tnull(login->session);

    //  The cookie the client kept is now worthless
    web = allocTestWeb(host, cookie);
    tfalse(webAuthenticate(web));
    tnull(web->user);
    freeTestWeb(web);

    //  Logging out again is harmless
    webLogout(login);

    rFree(cookie);
    freeTestWeb(login);
}

/*
    Logging in again replaces the session rather than reusing it, so a session id captured before
    login cannot be used afterwards.
 */
static void testSessionRotatedOnLogin(WebHost *host)
{
    Web  *web, *probe;
    char *first, *second;

    web = allocTestWeb(host, NULL);
    ttrue(webLogin(web, "alice", "admin"));
    first = sessionCookie(web);

    ttrue(webLogin(web, "alice", "admin"));
    second = sessionCookie(web);

    tfalse(smatch(first, second));

    //  The superseded session no longer authenticates
    probe = allocTestWeb(host, first);
    tfalse(webAuthenticate(probe));
    freeTestWeb(probe);

    //  The current one does
    probe = allocTestWeb(host, second);
    ttrue(webAuthenticate(probe));
    freeTestWeb(probe);

    rFree(first);
    rFree(second);
    webDestroySession(web);
    freeTestWeb(web);
}

static void fiberMain(void *arg)
{
    WebHost *host;
    Json    *config;

    webInit();
    config = jsonParseFile("web.json5", NULL, 0);
    tnotnull(config);
    if (config && (host = webAllocHost(config, 0)) != 0) {
        testLogin(host);
        testLoginRefused(host);
        testAuthenticateFromCookie(host);
        testAuthenticateRejected(host);
        testRoleChangeRevokesSession(host);
        testDeletedUserRevokesSession(host);
        testLogout(host);
        testSessionRotatedOnLogin(host);
        webFreeHost(host);
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
