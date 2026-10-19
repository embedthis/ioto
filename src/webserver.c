/*
    webserver.c - Configure the embedded web server

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include "ioto.h"

#if SERVICES_WEB
/*********************************** Forwards *********************************/

static int parseShow(cchar *arg);
#if SERVICES_DATABASE
static void checkPublishedPassword(WebHost *host, const DbItem *item);
static void checkPublishedPasswords(WebHost *host);
static void userChanged(WebHost *host, Db *db, DbModel *model, DbItem *item, DbParams *params, cchar *cmd,
                        int events);

/*
    The password the 3.1 http template gave its seeded accounts. Its hashes were published.
 */
#define PUBLISHED_PASSWORD "demo"
#endif

/************************************* Code ***********************************/

PUBLIC int ioInitWeb(void)
{
    WebHost *webHost;
    cchar   *webShow;
    char    *path;

    webInit();

    //  Expand "@" path prefixes in the documents directory
    path = rGetFilePath(jsonGet(ioto->config, 0, "web.documents", "site"));
    jsonSet(ioto->config, 0, "web.documents", path, JSON_STRING);
    rFree(path);

    if (ioto->cmdListen) {
        Json *listen = jsonParse(ioto->cmdListen, 0);
        if (listen) {
            jsonBlend(ioto->config, 0, "web.listen", listen, 0, 0, 0);
        } else {
            rError("web", "Failed to parse listen endpoints: %s", ioto->cmdListen);
            return R_ERR_CANT_READ;
        }
    }

    webShow = ioto->cmdWebShow ? ioto->cmdWebShow : jsonGet(ioto->config, 0, "log.show", "");

    if ((webHost = webAllocHost(ioto->config, parseShow(webShow))) == 0) {
        return R_ERR_CANT_INITIALIZE;
    }
#if SERVICES_DATABASE
    {
        cchar *url;
        if ((url = jsonGet(ioto->config, 0, "web.auth.login", 0)) != 0) {
            webAddAction(webHost, url, webLoginUser, NULL);
        }
        if ((url = jsonGet(ioto->config, 0, "web.auth.logout", 0)) != 0) {
            webAddAction(webHost, url, webLogoutUser, NULL);
        }
    }
#endif
#if ESP32
    webSetHostDefaultIP(webHost, rGetIP());
#endif

    if (webStartHost(webHost) < 0) {
        webFreeHost(webHost);
        return R_ERR_CANT_OPEN;
    }
    ioto->webHost = webHost;
#if SERVICES_DATABASE
    if (ioto->db && jsonGet(ioto->config, 0, "web.auth.login", 0)) {
        //  The User table is the source of truth: a change drops the cached web user and its sessions
        dbAddCallback(ioto->db, (DbCallbackProc) userChanged, "User", webHost, DB_ON_CHANGE);
        checkPublishedPasswords(webHost);
    }
#endif
    return 0;
}

PUBLIC void ioTermWeb(void)
{
    if (ioto->webHost) {
#if SERVICES_DATABASE
        if (ioto->db) {
            dbRemoveCallback(ioto->db, (DbCallbackProc) userChanged, "User", ioto->webHost);
        }
#endif
        webStopHost(ioto->webHost);
        webFreeHost(ioto->webHost);
    }
    ioto->webHost = 0;
    webTerm();
}

PUBLIC int ioRestartWeb(void)
{
    webStopHost(ioto->webHost);
    return webStartHost(ioto->webHost);
}

/*
    Parse the HTTP show command argument
 */
static int parseShow(cchar *arg)
{
    int show;

    show = 0;
    if (!arg) {
        return show;
    }
    if (schr(arg, 'H')) {
        show |= WEB_SHOW_REQ_HEADERS;
    }
    if (schr(arg, 'B')) {
        show |= WEB_SHOW_REQ_BODY;
    }
    if (schr(arg, 'h')) {
        show |= WEB_SHOW_RESP_HEADERS;
    }
    if (schr(arg, 'b')) {
        show |= WEB_SHOW_RESP_BODY;
    }
    return show;
}

#if SERVICES_DATABASE
/*
    Serialize the item privately before a yielding write. Partial responses are not signature-validated or finalized
    here.
 */
PUBLIC ssize webWriteItem(Web *web, const DbItem *item)
{
    if (!item) {
        return 0;
    }
    return webWriteJson(web, dbJson(item));
}

/*
    Serialize the grid privately before yielding writes. Do not finalize the response.
 */
PUBLIC ssize webWriteItems(Web *web, RList *items)
{
    DbItem *item;
    RBuf   *buf;
    char   *str;
    ssize  index, rc;

    if (!items) {
        return 0;
    }
    buf = rAllocBuf(0);
    rPutCharToBuf(buf, '[');
    for (ITERATE_ITEMS(items, item, index)) {
        if (!item || (str = jsonToString(dbJson(item), 0, NULL, JSON_JSON)) == 0) {
            continue;
        }
        if (rGetBufLength(buf) > 1) {
            rPutCharToBuf(buf, ',');
        }
        rPutStringToBuf(buf, str);
        rFree(str);
    }
    rPutCharToBuf(buf, ']');
    rc = webWrite(web, rGetBufStart(buf), rGetBufLength(buf));
    rFreeBuf(buf);
    return rc;
}

/*
    Write a database item. DOES finalize the response.
 */
PUBLIC ssize webWriteValidatedItem(Web *web, const DbItem *item, cchar *sigKey)
{
    ssize rc;

    if (!item) {
        return 0;
    }
    if (web->host->signatures) {
        rc = webWriteValidatedJson(web, dbJson(item), sigKey);
    } else {
        rc = webWriteItem(web, item);
    }
    webFinalize(web);
    return rc;
}

/*
    Write a validated database grid as a response. Finalizes the response.
 */
PUBLIC ssize webWriteValidatedItems(Web *web, RList *items, cchar *sigKey)
{
    Json   *signatures;
    DbItem *item;
    ssize  index;
    int    sid;

    if (!items) {
        return 0;
    }
    signatures = web->host->signatures;
    if (!signatures) {
        rError("web", "Missing signatures or signature ID");
        return 0;
    }
    if (sigKey) {
        sid = jsonGetId(signatures, 0, sigKey);
    } else {
        sid = jsonGetId(signatures, web->signature, "response.of");
    }
    if (sid < 0) {
        webWriteResponse(web, 0, "Invalid signature for response");
        return R_ERR_BAD_STATE;
    }
    webBuffer(web, 0);
    rPutCharToBuf(web->buffer, '[');

    for (ITERATE_ITEMS(items, item, index)) {
        if (item) {
            if (!webValidateSignature(web, web->buffer, dbJson(item), 0, sid, 0, "response")) {
                return R_ERR_BAD_ARGS;
            }
            rPutCharToBuf(web->buffer, ',');
        }
    }
    // Trim trailing comma
    if (rGetBufLength(web->buffer) > 1) {
        rAdjustBufEnd(web->buffer, -1);
    }
    rPutCharToBuf(web->buffer, ']');
    webFinalize(web);
    return (ssize) rGetBufLength(web->buffer);
}

/*
    Default login action. This is designed for web page use and redirects as a response (i.e. not for SPAs).
    Every failure gets the same response, log line and password work, so neither reveals which usernames exist.
 */
PUBLIC void webLoginUser(Web *web)
{
    /*
        SECURITY Acceptable:: Users should utilize the anti-CSRF token protection provided by the web server.
     */
    const DbItem *user;
    cchar        *password, *role, *username;
    bool         ok;

    if (webAuthBlocked(web)) {
        webLogAuthFailure(web, "too many failures");
        webWriteResponse(web, 429, "Too many failed logins");
        return;
    }
    username = webGetVar(web, "username", 0);
    password = webGetVar(web, "password", 0);
    user = username ? dbFindOne(ioto->db, "User", DB_PROPS("username", username), DB_PARAMS()) : 0;

    //  An unknown user is checked against a decoy hash
    ok = webCheckPassword(web->host, username, password, user ? dbField(user, "password") : 0);

    if (ok && webUserHasCredential(webLookupUser(web->host, username))) {
        rError("auth", "Database user \"%s\" has the name of a web.json5 user, so it cannot log in", username);
        ok = 0;
    }
    if (ok && smatch(password, PUBLISHED_PASSWORD)) {
        rError("auth", "Refused login as \"%s\": its password is the published default \"%s\". Set a new password.",
               username, PUBLISHED_PASSWORD);
        ok = 0;
    }
    if (!ok) {
        webAuthFailed(web);
        webLogAuthFailure(web, "login failed");
        webWriteResponse(web, 401, "Invalid username or password");
        return;
    }
    role = dbField(user, "role");
    if (!webLogin(web, username, role)) {
        webWriteResponse(web, 400, "Unknown user role");
    } else {
        webRedirectLocal(web, 302, "/");
    }
}

PUBLIC void webLogoutUser(Web *web)
{
    // Require POST so links and images cannot log users out.
    if (!smatch(web->method, "POST")) {
        webError(web, 405, "Logout requires POST");
        return;
    }
    webLogout(web);
    webRedirectLocal(web, 302, "/");
}

/*
    A changed or removed User row drops the cached web user, which destroys its sessions. The next login
    rebuilds it with the role the database now holds. Users defined in web.json5 are not database users.
 */
static void userChanged(WebHost *host, Db *db, DbModel *model, DbItem *item, DbParams *params, cchar *cmd,
                        int events)
{
    WebUser *user;
    cchar   *username;

    (void) db;
    (void) model;
    (void) params;
    (void) events;

    if (!smatch(cmd, "remove")) {
        checkPublishedPassword(host, item);
    }
    if (smatch(cmd, "create") || (username = dbField(item, "username")) == 0) {
        return;
    }
    if ((user = webLookupUser(host, username)) == 0 || webUserHasCredential(user)) {
        return;
    }
    if (smatch(cmd, "remove") || !smatch(user->role, dbField(item, "role"))) {
        webRemoveUser(host, username);
    }
}

/*
    Report an account whose password is the published default. Such an account cannot log in. The 3.1
    template made its hashes over the bare password; the password utility makes them over username:realm:password.
 */
static void checkPublishedPassword(WebHost *host, const DbItem *item)
{
    cchar *hash, *username;

    username = dbField(item, "username");
    hash = dbField(item, "password");
    if (!username || !hash) {
        return;
    }
    if (cryptCheckPassword(PUBLISHED_PASSWORD, hash) || webCheckPassword(host, username, PUBLISHED_PASSWORD, hash)) {
        rError("auth", "User \"%s\" has the published default password \"%s\" and cannot log in. "
               "Set a new password or remove the account.", username, PUBLISHED_PASSWORD);
    }
}

static void checkPublishedPasswords(WebHost *host)
{
    RList  *users;
    DbItem *item;
    int    next;

    if ((users = dbFind(ioto->db, "User", NULL, NULL)) == 0) {
        return;
    }
    for (ITERATE_ITEMS(users, item, next)) {
        checkPublishedPassword(host, item);
    }
    rFreeList(users);
}
#endif /* SERVICES_DATABASE */

#else
void dummyWeb(void)
{
}
#endif /* SERVICES_WEB */
/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
