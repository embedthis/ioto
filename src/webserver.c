/*
    webserver.c - Configure the embedded web server

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include "ioto.h"

#if SERVICES_WEB
/*********************************** Forwards *********************************/

static int parseShow(cchar *arg);

/************************************* Code ***********************************/

PUBLIC int ioInitWeb(void)
{
    WebHost *webHost;
    cchar   *webShow;
    char    *path;

    webInit();

    /*
        Rebase relative documents and upload directories under "state"
     */
    path = rGetFilePath(jsonGet(ioto->config, 0, "web.documents", "site"));
    jsonSet(ioto->config, 0, "web.documents", path, JSON_STRING);
    rFree(path);

    path = rGetFilePath(jsonGet(ioto->config, 0, "web.upload.dir", "tmp"));
    jsonSet(ioto->config, 0, "web.upload.dir", path, JSON_STRING);
    rFree(path);

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
#if ESP32 || FREERTOS
    webSetHostDefaultIP(webHost, rGetIP());
#endif

    if (webStartHost(webHost) < 0) {
        webFreeHost(webHost);
        return R_ERR_CANT_OPEN;
    }
    ioto->webHost = webHost;
    return 0;
}

PUBLIC void ioTermWeb(void)
{
    if (ioto->webHost) {
        webStopHost(ioto->webHost);
        webFreeHost(ioto->webHost);
    }
    ioto->webHost = 0;
    webTerm();
}

PUBLIC void ioRestartWeb(void)
{
    webStopHost(ioto->webHost);
    webStartHost(ioto->webHost);
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
    Write a database item as part of a response. Does not finalize the response.
    Not validated against the API signature as it could be only part of the response.
 */
PUBLIC ssize webWriteItem(Web *web, const DbItem *item)
{
    if (!item) {
        return 0;
    }
    return webWrite(web, dbString(item, JSON_JSON), 0);
}

/*
    Write a database grid of items as part of a response. Does not finalize the response.
 */
PUBLIC ssize webWriteItems(Web *web, RList *items)
{
    DbItem *item;
    ssize  index, rc, wrote;
    bool   prior;

    if (!items) {
        return 0;
    }
    rc = 0;
    prior = 0;
    webWrite(web, "[", 1);
    for (ITERATE_ITEMS(items, item, index)) {
        if (!item) {
            continue;
        }
        if (prior) {
            rc += webWrite(web, ",", 0);
        }
        if ((wrote = webWriteItem(web, item)) <= 0) {
            continue;
        }
        rc += wrote;
        prior = 1;
    }
    rc += webWrite(web, "]", 1);
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
    if (signatures) {
        if (sigKey) {
            sid = jsonGetId(signatures, 0, sigKey);
        } else {
            sid = jsonGetId(signatures, web->signature, "response.of");
        }
        if (sid < 0) {
            webWriteResponse(web, 0, "Invalid signature for response");
            return R_ERR_BAD_STATE;
        }
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
 */
PUBLIC void webLoginUser(Web *web)
{
    /*
        SECURITY Acceptable:: Users should utilize the anti-CSRF token protection provided by the web server.
     */
    const DbItem *user;
    cchar        *password, *role, *username;

    username = webGetVar(web, "username", 0);
    password = webGetVar(web, "password", 0);
    user = dbFindOne(ioto->db, "User", DB_PROPS("username", username), DB_PARAMS());

    if (!user || !cryptCheckPassword(password, dbField(user, "password"))) {
        //  Security: a generic message and fixed delay defeats username enumeration and timing attacks.
        rSleep(500);
        webWriteResponse(web, 401, "Invalid username or password");
        return;
    }
    role = dbField(user, "role");
    if (!webLogin(web, username, role)) {
        webWriteResponse(web, 400, "Unknown user role");
    } else {
        webRedirect(web, 302, "/");
    }
}

PUBLIC void webLogoutUser(Web *web)
{
    webLogout(web);
    webRedirect(web, 302, "/");
}
#endif /* SERVICES_DATABASE */

#else
void dummyWeb(void)
{
}
#endif /* SERVICES_WEB */
/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
