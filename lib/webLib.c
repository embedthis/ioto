/*
 * Embedthis Web Library Source
 */

#include "web.h"

#if ME_COM_WEB



/********* Start of file ../../../src/auth.c ************/

/*
    auth.c -- Authorization Management

    This modules supports a simple role based authorization scheme.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



/************************************ Code ************************************/
#if ME_WEB_AUTH
/*
    Authenticate the current request.

    This checks if the request has a current session by using the request cookie.
    Returns true if authenticated.
    Residual: set web->authenticated.
 */
PUBLIC bool webAuthenticate(Web *web)
{
    if (web->authChecked) {
        return web->authenticated;
    }
    web->authChecked = 1;

    if (web->cookie && webGetSession(web, 0) != 0) {
        /*
            SECURITY Acceptable:: Retrieve authentication state from the session storage.
            Faster than re-authenticating.
         */
        if ((web->username = webGetSessionVar(web, WEB_SESSION_USERNAME, 0)) != 0) {
            web->role = webGetSessionVar(web, WEB_SESSION_ROLE, 0);
            if (web->role && web->host->roles >= 0) {
                if ((web->roleId = jsonGetId(web->host->config, web->host->roles, web->role)) < 0) {
                    rError("web", "Unknown role in webAuthenticate: %s", web->role);
                } else {
                    web->authenticated = 1;
                    return 1;
                }
            }
        }
    }
    return 0;
}

PUBLIC bool webIsAuthenticated(Web *web)
{
    if (!web->authChecked) {
        return webAuthenticate(web);
    }
    return web->authenticated;
}

/*
    Check if the authenticated user's role is sufficient to perform the required role's activities
 */
PUBLIC bool webCan(Web *web, cchar *requiredRole)
{
    WebHost *host;
    int     roleId;

    assert(web);
    assert(requiredRole);

    if (!requiredRole || *requiredRole == '\0' || smatch(requiredRole, "public")) {
        return 1;
    }
    host = web->host;

    if (!web->authenticated && !webAuthenticate(web)) {
        webError(web, 401, "Access Denied. User not logged in.");
        return 0;
    }
    roleId = jsonGetId(host->config, host->roles, requiredRole);
    if (roleId < 0 || roleId > web->roleId) {
        return 0;
    }
    return 1;
}

/*
    Return the role of the authenticated user
 */
PUBLIC cchar *webGetRole(Web *web)
{
    return jsonGet(web->host->config, web->roleId, 0, 0);
}

/*
    Login and authorize a user with a given role.
    This creates the login session and defines a session cookie for responses.
    This assumes the caller has already validated the user password.
 */
PUBLIC bool webLogin(Web *web, cchar *username, cchar *role)
{
    assert(web);
    assert(username);
    assert(role);

    web->username = 0;
    web->role = 0;
    web->roleId = -1;

    webRemoveSessionVar(web, WEB_SESSION_USERNAME);

    if ((web->roleId = jsonGetId(web->host->config, web->host->roles, role)) < 0) {
        rError("web", "Unknown role %s", role);
        return 0;
    }
    webCreateSession(web);

    web->username = webSetSessionVar(web, WEB_SESSION_USERNAME, username);
    web->role = webSetSessionVar(web, WEB_SESSION_ROLE, role);
    return 1;
}

/*
    Logout the authenticated user by destroying the user session
 */
PUBLIC void webLogout(Web *web)
{
    assert(web);

    web->username = 0;
    web->role = 0;
    web->roleId = -1;
    webRemoveSessionVar(web, WEB_SESSION_USERNAME);
    webDestroySession(web);
}
#endif /* ME_WEB_AUTH */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file ../../../src/file.c ************/

/*
    file.c - File handler for serving static content

    Handles: Get, head, post, put and delete methods.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



/************************************ Locals **********************************/

typedef struct stat FileInfo;

/************************************ Forwards *********************************/

static int deleteFile(Web *web, cchar *path);
static int getFile(Web *web, cchar *path, FileInfo *info);
static int putFile(Web *web, cchar *path);
static int redirectToDir(Web *web);

/************************************* Code ***********************************/

PUBLIC int webFileHandler(Web *web)
{
    FileInfo info;
    char     *path;
    int      rc;

    // SECURITY Acceptable:: the path is already validated and normalized in webValidateRequest / webNormalizePath
    path = sjoin(webGetDocs(web->host), web->path, NULL);

    web->exists = stat(path, &info) == 0;
    web->ext = strrchr(path, '.');

    if (web->get || web->head || web->post) {
        rc = getFile(web, path, &info);

    } else if (web->put) {
        rc = putFile(web, path);

    } else if (web->del) {
        rc = deleteFile(web, path);

    } else {
        rc = webError(web, 405, "Unsupported method");
    }
    //  Delay free until here after writing headers to web->ext is valid
    rFree(path);
    return rc;
}

static int getFile(Web *web, cchar *path, FileInfo *info)
{
    char date[32], *lpath;
    int  fd;

    if (!web->exists) {
        webHook(web, WEB_HOOK_NOT_FOUND);
        if (!web->finalized) {
            return webError(web, 404, "Cannot locate document");
        }
        return 0;
    }
    lpath = 0;
    if (S_ISDIR(info->st_mode)) {
        //  Directory. If request does not end with "/", must do an external redirect.
        if (!sends(path, "/")) {
            return redirectToDir(web);
        }
        //  Internal redirect to the directory index
        path = lpath = sjoin(path, web->host->index, NULL);
        web->exists = stat(path, info) == 0;
        web->ext = strrchr(path, '.');
    }
    if ((fd = open(path, O_RDONLY | O_BINARY, 0)) < 0) {
        rFree(lpath);
        return webError(web, 404, "Cannot open document");
    }

    if (web->since && info->st_mtime <= web->since) {
        //  Request has not been modified since the client last retrieved the document
        web->txLen = 0;
        web->status = 304;
    } else {
        web->status = 200;
        web->txLen = info->st_size;
    }
    if (info->st_mtime > 0) {
        webAddHeader(web, "Last-Modified", "%s", webDate(date, info->st_mtime));
    }

    //  ETag is a hash of the file's inode, size and last modified time.
    webAddHeader(web, "ETag", "\"%Ld\"", (uint64) info->st_ino ^ (uint64) info->st_size ^ (uint64) info->st_mtime);

    if (web->head) {
        webFinalize(web);
        close(fd);
        rFree(lpath);
        return 0;
    }
    if (web->txLen > 0 && webSendFile(web, fd) < 0) {
        close(fd);
        rFree(lpath);
        return R_ERR_CANT_WRITE;
    }
    close(fd);
    rFree(lpath);
    return 0;
}

/*
    We can't just serve the index even if we know it exists.
    Must do an external redirect to the directory as required by the spec.
    Must preserve query and ref.
 */
static int redirectToDir(Web *web)
{
    RBuf *buf;
    char *url;

    buf = rAllocBuf(0);
    rPutStringToBuf(buf, web->path);
    rPutCharToBuf(buf, '/');
    if (web->query) {
        rPutToBuf(buf, "?%s", web->query);
    }
    if (web->hash) {
        rPutToBuf(buf, "#%s", web->hash);
    }
    url = rBufToStringAndFree(buf);
    webRedirect(web, 301, url);
    rFree(url);
    return 0;
}

static int putFile(Web *web, cchar *path)
{
    char  buf[ME_BUFSIZE];
    ssize nbytes;
    int   fd;

    if ((fd = open(path, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, 0600)) < 0) {
        return webError(web, 404, "Cannot open document");
    }
    while ((nbytes = webRead(web, buf, sizeof(buf))) > 0) {
        if (write(fd, buf, nbytes) != nbytes) {
            return webError(web, 500, "Cannot put document");
        }
    }
    return (int) webWriteResponse(web, web->exists ? 204 : 201, "Document successfully updated");
}

static int deleteFile(Web *web, cchar *path)
{
    if (!web->exists) {
        return webError(web, 404, "Cannot locate document");
    }
    unlink(path);
    return (int) webWriteResponse(web, 204, "Document successfully deleted");
}

PUBLIC ssize webSendFile(Web *web, int fd)
{
    ssize written, nbytes;
    char  buf[ME_BUFSIZE];

    for (written = 0; written < web->txLen; ) {
        if ((nbytes = read(fd, buf, sizeof(buf))) < 0) {
            return webError(web, 404, "Cannot read document");
        }
        if ((nbytes = webWrite(web, buf, nbytes)) < 0) {
            return webNetError(web, "Cannot send file");
        }
        written += nbytes;
    }
    return written;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file ../../../src/host.c ************/

/*
    host.c - Web Host. This is responsible for a set of listening endpoints.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



/************************************ Forwards *********************************/

static WebListen *allocListen(WebHost *host, cchar *endpoint);
static RHash *createMethodsHash(cchar *list);
static void freeListen(WebListen *listen);
static int getTimeout(WebHost *host, cchar *field, cchar *defaultValue);
static void initMethods(WebHost *host);
static void initRedirects(WebHost *host);
static void initRoutes(WebHost *host);
static void loadMimeTypes(WebHost *host);
static cchar *uploadDir(void);

/************************************* Code ***********************************/

PUBLIC int webInit(void)
{
    return 0;
}

PUBLIC void webTerm(void)
{
}

PUBLIC WebHost *webAllocHost(Json *config, int flags)
{
    WebHost *host;
    cchar   *path, *show;
    char    *errorMsg;

    if ((host = rAllocType(WebHost)) == 0) {
        return 0;
    }
    if (flags == 0 && (show = getenv("WEB_SHOW")) != 0) {
        if (schr(show, 'H')) {
            flags |= WEB_SHOW_REQ_HEADERS;
        }
        if (schr(show, 'B')) {
            flags |= WEB_SHOW_REQ_BODY;
        }
        if (schr(show, 'h')) {
            flags |= WEB_SHOW_RESP_HEADERS;
        }
        if (schr(show, 'b')) {
            flags |= WEB_SHOW_RESP_BODY;
        }
    }
    host->flags = flags;
    host->actions = rAllocList(0, 0);
    host->listeners = rAllocList(0, 0);
    host->sessions = rAllocHash(0, 0);
    host->webs = rAllocList(0, 0);

    if (!config) {
        if ((config = jsonParseFile(ME_WEB_CONFIG, &errorMsg, 0)) == 0) {
            rError("config", "%s", errorMsg);
            rFree(errorMsg);
            rFree(host);
            return 0;
        }
        jsonLock(config);
        host->freeConfig = 1;
    }
    host->config = config;

    /*
        Parse a signatures.json file that is used to validate REST requests to the web server
     */
    if (jsonGetBool(host->config, 0, "web.signatures.enable", 0)) {
        path = jsonGet(host->config, 0, "web.signatures.path", 0);
        if ((host->signatures = jsonParseFile(path, &errorMsg, 0)) == 0) {
            rError("web", "Cannot parse signatures file: %s", errorMsg);
            rFree(host);
            return 0;
        }
        host->strictSignatures = jsonGetBool(host->config, 0, "web.signatures.strict", 0);
    }

    host->index = jsonGet(host->config, 0, "web.index", "index.html");
    host->parseTimeout = getTimeout(host, "web.timeouts.parse", "5secs");
    host->inactivityTimeout = getTimeout(host, "web.timeouts.inactivity", "5mins");
    host->requestTimeout = getTimeout(host, "web.timeouts.request", "5mins");
    host->sessionTimeout = getTimeout(host, "web.timeouts.session", "30mins");

#if ME_WEB_LIMITS
    host->maxBuffer = svalue(jsonGet(host->config, 0, "web.limits.buffer", "64K"));
    host->maxBody = svalue(jsonGet(host->config, 0, "web.limits.body", "100K"));
    host->maxConnections = svalue(jsonGet(host->config, 0, "web.limits.connections", "100"));
    host->maxHeader = svalue(jsonGet(host->config, 0, "web.limits.header", "10K"));
    host->maxSessions = svalue(jsonGet(host->config, 0, "web.limits.sessions", "20"));
    host->maxUpload = svalue(jsonGet(host->config, 0, "web.limits.upload", "20MB"));
    host->maxUploads = svalue(jsonGet(host->config, 0, "web.limits.uploads", "0"));
#endif

    host->docs = rGetFilePath(jsonGet(host->config, 0, "web.documents", "@site"));
    host->name = jsonGet(host->config, 0, "web.name", 0);
    host->uploadDir = jsonGet(host->config, 0, "web.upload.dir", uploadDir());
    host->sessionCookie = jsonGet(host->config, 0, "web.sessions.cookie", WEB_SESSION_COOKIE);
    host->sameSite = jsonGet(host->config, 0, "web.sessions.sameSite", "Lax");
    host->httpOnly = jsonGetBool(host->config, 0, "web.sessions.httpOnly", 1);
    host->roles = jsonGetId(host->config, 0, "web.auth.roles");
    host->headers = jsonGetId(host->config, 0, "web.headers");

    host->webSocketsMaxMessage = svalue(jsonGet(host->config, 0, "web.limits.maxMessage", "100K"));
    host->webSocketsMaxFrame = svalue(jsonGet(host->config, 0, "web.limits.maxFrame", "100K"));
    host->webSocketsValidateUTF = jsonGetBool(host->config, 0, "web.webSockets.validateUTF", 0);
    host->webSocketsPingPeriod = svalue(jsonGet(host->config, 0, "web.webSockets.ping", "never"));
    host->webSocketsProtocol = jsonGet(host->config, 0, "web.webSockets.protocol", "chat");
    host->webSocketsEnable = jsonGetBool(host->config, 0, "web.webSockets.enable", 1);

    initMethods(host);
    initRoutes(host);
    initRedirects(host);
    loadMimeTypes(host);
    webInitSessions(host);
    return host;
}

PUBLIC void webFreeHost(WebHost *host)
{
    WebListen   *listen;
    WebAction   *action;
    WebRoute    *route;
    WebRedirect *redirect;
    Web         *web;
    RName       *np;
    int         next;

    rStopEvent(host->sessionEvent);

    for (ITERATE_ITEMS(host->listeners, listen, next)) {
        freeListen(listen);
    }
    rFreeList(host->listeners);

    for (ITERATE_ITEMS(host->webs, web, next)) {
        webFree(web);
    }
    for (ITERATE_ITEMS(host->redirects, redirect, next)) {
        rFree(redirect);
    }
    rFreeList(host->webs);
    rFreeList(host->redirects);
    rFreeHash(host->methods);

    for (ITERATE_ITEMS(host->routes, route, next)) {
        if (route->methods != host->methods) {
            rFreeHash(route->methods);
        }
        rFree(route);
    }
    rFreeList(host->routes);

    for (ITERATE_ITEMS(host->actions, action, next)) {
        rFree(action->match);
        rFree(action->role);
        rFree(action);
    }
    rFreeList(host->actions);

    for (ITERATE_NAMES(host->sessions, np)) {
        rRemoveName(host->sessions, np->name);
        webFreeSession(np->value);
    }
    rFreeHash(host->sessions);
    rFreeHash(host->mimeTypes);
    if (host->freeConfig) {
        jsonFree(host->config);
    }
    if (host->signatures) {
        jsonFree(host->signatures);
        host->signatures = 0;
    }
    rFree(host->docs);
    rFree(host->ip);
    rFree(host);
}

PUBLIC int webStartHost(WebHost *host)
{
    Json      *json;
    WebListen *listen;
    JsonNode  *np;
    cchar     *endpoint;

    if (!host || !host->listeners) return 0;
    json = host->config;

    for (ITERATE_JSON_KEY(json, 0, "web.listen", np, id)) {
        endpoint = jsonGet(json, id, 0, 0);
        if ((listen = allocListen(host, endpoint)) == 0) {
            return R_ERR_CANT_OPEN;
        }
        rAddItem(host->listeners, listen);
    }
    return 0;
}

PUBLIC void webStopHost(WebHost *host)
{
    WebListen *listen;
    Web       *web;
    int       next;

    rStopEvent(host->sessionEvent);

    for (ITERATE_ITEMS(host->listeners, listen, next)) {
        rCloseSocket(listen->sock);
    }
    for (ITERATE_ITEMS(host->webs, web, next)) {
        rCloseSocket(web->sock);
    }
}

/*
    Create the listening endpoint and start listening for requests
 */
static WebListen *allocListen(WebHost *host, cchar *endpoint)
{
    WebListen *listen;
    RSocket   *sock;
    char      *hostname, *sport, *scheme, *tok, *end;
    int       port;

    tok = sclone(endpoint);
    scheme = sptok(tok, "://", &hostname);

    if (!hostname) {
        hostname = scheme;
        scheme = NULL;
    } else if (*hostname == '\0') {
        hostname = "localhost";
    }
    hostname = sptok(hostname, ":", &sport);
    if (!sport) {
        sport = smatch(scheme, "https") ? "443" : "80";
    }
    port = (int) strtol(sport, &end, 10);
    if (*end) {
        rError("web", "Bad characters in port of endpoint \"%s\"", sport);
        rFree(tok);
        return 0;
    }

    if (port <= 0 || port > 65535) {
        rError("web", "Bad or missing port %d in Listen directive", port);
        rFree(tok);
        return 0;
    }
    if (*hostname == 0) {
        hostname = NULL;
    }

    if ((listen = rAllocType(WebListen)) == 0) {
        rError("web", "Cannot allocate memory for WebListen");
        return 0;
    }
    listen->host = host;
    listen->endpoint = sclone(endpoint);
    rInfo("web", "Listening %s", endpoint);


    listen->sock = sock = rAllocSocket();
    listen->port = port;

#if ME_COM_SSL
    if (smatch(scheme, "https")) {
        webSecureEndpoint(listen);
    }
#endif
    if (rListenSocket(sock, hostname, port, (RSocketProc) webAlloc, listen) < 0) {
        rError("web", "Cannot listen on %s:%d", hostname ? hostname : "*", port);
        rFree(tok);
        return 0;
    }
    rFree(tok);
    return listen;
}

static void freeListen(WebListen *listen)
{
    rFreeSocket(listen->sock);
    rFree(listen->endpoint);
    rFree(listen);
}

#if ME_COM_SSL
PUBLIC int webSecureEndpoint(WebListen *listen)
{
    Json  *config;
    cchar *ciphers;
    char  *authority, *certificate, *key;
    bool  verifyClient, verifyIssuer;
    int   rc;

    config = listen->host->config;


    ciphers = jsonGet(config, 0, "tls.ciphers", 0);
    if (ciphers) {
        char *clist = jsonToString(config, 0, "tls.ciphers", JSON_BARE);
        rSetSocketDefaultCiphers(clist);
        rFree(clist);
    }
    verifyClient = jsonGetBool(config, 0, "tls.verify.client", 0);
    verifyIssuer = jsonGetBool(config, 0, "tls.verify.issuer", 0);
    rSetSocketDefaultVerify(verifyClient, verifyIssuer);

    authority = rGetFilePath(jsonGet(config, 0, "tls.authority", 0));
    certificate = rGetFilePath(jsonGet(config, 0, "tls.certificate", 0));
    key = rGetFilePath(jsonGet(config, 0, "tls.key", 0));

    rc = 0;
    if (key && certificate) {
        if (rAccessFile(key, R_OK) < 0) {
            rError("web", "Cannot access certificate key %s", key);
            rc = R_ERR_CANT_OPEN;
        } else if (rAccessFile(certificate, R_OK) < 0) {
            rError("web", "Cannot access certificate %s", certificate);
            rc = R_ERR_CANT_OPEN;
        } else if (authority && rAccessFile(authority, R_OK) < 0) {
            rError("web", "Cannot access authority %s", authority);
            rc = R_ERR_CANT_OPEN;
        }
    }
    if (rc == 0) {
        rSetSocketCerts(listen->sock, authority, key, certificate, NULL);
    } else {
        rError("web", "Secure endpoint %s is not yet ready as it does not have a certificate or key.",
               listen->endpoint);
    }
    rFree(authority);
    rFree(certificate);
    rFree(key);
    return rc;
}
#endif

/*
    Get a timeout value in milliseconds. If the value is greater than MAXINT / TPS, return MAXINT / TPS.
    This is to prevent overflow.
 */
static int getTimeout(WebHost *host, cchar *field, cchar *defaultValue)
{
    uint64 value;

    value = svalue(jsonGet(host->config, 0, field, defaultValue));
    if (value > MAXINT / TPS) {
        return MAXINT / TPS;
    }
    return (int) value * TPS;
}

static cchar *uploadDir(void)
{
#if ME_WIN_LIKE
    return getenv("TEMP");
#else
    return "/tmp";
#endif
}

static void initMethods(WebHost *host)
{
    cchar *methods;

    methods = jsonGet(host->config, 0, "web.headers.Access-Control-Allow-Methods", 0);
    if (methods == 0) {
        methods = "GET, POST";
    }
    host->methods = createMethodsHash(methods);
}

static RHash *createMethodsHash(cchar *list)
{
    RHash *hash;
    char  *method, *methods, *tok;

    methods = sclone(list);
    hash = rAllocHash(0, R_TEMPORAL_NAME);
    for (method = stok(methods, " \t,", &tok); method; method = stok(NULL, " \t,", &tok)) {
        method = strim(method, "'\"", R_TRIM_BOTH);
        rAddName(hash, method, "true", 0);
    }
    rFree(methods);
    return hash;
}

/*
    Default set of mime types. Can be overridden via the web.json5
 */
static cchar *MimeTypes[] = {
    ".avi", "video/x-msvideo",
    ".bin", "application/octet-stream",
    ".class", "application/java",
    ".css", "text/css",
    ".eps", "application/postscript",
    ".gif", "image/gif",
    ".gz", "application/gzip",
    ".htm", "text/html",
    ".html", "text/html",
    ".ico", "image/vnd.microsoft.icon",
    ".jar", "application/java",
    ".jpeg", "image/jpeg",
    ".jpg", "image/jpeg",
    ".js", "application/x-javascript",
    ".json", "application/json",
    ".mov", "video/quicktime",
    ".mp4", "video/mp4",
    ".mpeg", "video/mpeg",
    ".mpg", "video/mpeg",
    ".patch", "application/x-patch",
    ".pdf", "application/pdf",
    ".png", "image/png",
    ".ps", "application/postscript",
    ".qt", "video/quicktime",
    ".rtf", "application/rtf",
    ".svg", "image/svg+xml",
    ".tgz", "application/x-tgz",
    ".tif", "image/tiff",
    ".tiff", "image/tiff",
    ".txt", "text/plain",
    ".wav", "audio/x-wav",
    ".xml", "text/xml",
    ".z", "application/compress",
    ".zip", "application/zip",
    NULL, NULL,
};

/*
    Load mime types for the host. This uses the default mime types and then overlays the user defined
    mime types from the web.json.
 */
static void loadMimeTypes(WebHost *host)
{
    JsonNode *child;
    cchar    **mp;

    host->mimeTypes = rAllocHash(0, R_STATIC_VALUE | R_STATIC_NAME);
    /*
        Define default mime types
     */
    for (mp = MimeTypes; *mp; mp += 2) {
        rAddName(host->mimeTypes, mp[0], (void*) mp[1], 0);
    }
    /*
        Overwrite user specified mime types
     */
    for (ITERATE_JSON_KEY(host->config, 0, "web.mime", child, id)) {
        rAddName(host->mimeTypes, child->name, child->value, 0);
    }
}

/*
    Initialize the request routes for the host. Routes match a URL to a request handler and required authenticated role.
 */
static void initRoutes(WebHost *host)
{
    Json     *json;
    JsonNode *route, *routes;
    WebRoute *rp;
    cchar    *match;
    char     *methods;

    host->routes = rAllocList(0, 0);
    json = host->config;
    routes = jsonGetNode(json, 0, "web.routes");

    if (routes == 0) {
        rp = rAllocType(WebRoute);
        rp->match = "";
        rp->handler = "file";
        rp->methods = host->methods;
        rp->validate = 0;
        rAddItem(host->routes, rp);

    } else {
        for (ITERATE_JSON(json, routes, route, id)) {
            rp = rAllocType(WebRoute);

            /*
                Exact match if pattern non-empty and not a trailing "/"
                Empty routes match everything
                A match of "/" will do an exact match.
             */
            match = rp->match = jsonGet(json, id, "match", "");
            rp->exact = (!match || slen(match) == 0 ||
                         (slen(match) > 0 && match[slen(match) - 1] == '/' && !smatch(match, "/"))
                         ) ? 0 : 1;
            rp->role = jsonGet(json, id, "role", 0);
            rp->redirect = jsonGet(json, id, "redirect", 0);
            rp->trim = jsonGet(json, id, "trim", 0);
            rp->handler = jsonGet(json, id, "handler", "file");
            rp->stream = jsonGetBool(json, id, "stream", 0);
            rp->validate = jsonGetBool(json, id, "validate", 0);
            rp->xsrf = jsonGetBool(json, id, "xsrf", 0);

            if ((methods = jsonToString(json, id, "methods", 0)) != 0) {
                // Trim leading and trailing brackets
                methods[slen(methods) - 1] = '\0';
                rp->methods = createMethodsHash(&methods[1]);
                rFree(methods);
            } else {
                rp->methods = host->methods;
            }
            rAddItem(host->routes, rp);
        }
    }
}

static void initRedirects(WebHost *host)
{
    Json        *json;
    JsonNode    *redirects, *np;
    WebRedirect *redirect;
    cchar       *from, *to;
    int         status;

    json = host->config;
    redirects = jsonGetNode(json, 0, "web.redirect");
    if (redirects == 0) {
        return;
    }
    host->redirects = rAllocList(0, 0);

    for (ITERATE_JSON(json, redirects, np, id)) {
        from = jsonGet(json, id, "from", 0);
        status = jsonGetInt(json, id, "status", 301);
        to = jsonGet(json, id, "to", 0);
        if (!status || !to) {
            rError("web", "Bad redirection. Missing from, status or target");
            continue;
        }
        redirect = rAllocType(WebRedirect);
        redirect->from = from;
        redirect->to = to;
        redirect->status = status;
        rPushItem(host->redirects, redirect);
    }
}

/*
    Define an action routine. This binds a URL to a C function.
 */
PUBLIC void webAddAction(WebHost *host, cchar *match, WebProc fn, cchar *role)
{
    WebAction *action;

    action = rAllocType(WebAction);
    action->match = sclone(match);
    action->role = sclone(role);
    action->fn = fn;
    rAddItem(host->actions, action);
}

/*
    Set the web lifecycle for this host.
 */
PUBLIC void webSetHook(WebHost *host, WebHook hook)
{
    host->hook = hook;
}

PUBLIC void webSetHostDefaultIP(WebHost *host, cchar *ip)
{
    rFree(host->ip);
    host->ip = sclone(ip);
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file ../../../src/http.c ************/

/*
    http.c - Core HTTP request processing

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



/************************************ Locals **********************************/

static int64 conn = 0;      /* Connection sequence number */

#define isWhite(c) ((c) == ' ' || (c) == '\t')

static cchar validHeaderChars[128] = {
    ['A'] = 1, ['B'] = 1, ['C'] = 1, ['D'] = 1, ['E'] = 1, ['F'] = 1, ['G'] = 1,
    ['H'] = 1, ['I'] = 1, ['J'] = 1, ['K'] = 1, ['L'] = 1, ['M'] = 1, ['N'] = 1,
    ['O'] = 1, ['P'] = 1, ['Q'] = 1, ['R'] = 1, ['S'] = 1, ['T'] = 1, ['U'] = 1,
    ['V'] = 1, ['W'] = 1, ['X'] = 1, ['Y'] = 1, ['Z'] = 1,
    ['a'] = 1, ['b'] = 1, ['c'] = 1, ['d'] = 1, ['e'] = 1, ['f'] = 1, ['g'] = 1,
    ['h'] = 1, ['i'] = 1, ['j'] = 1, ['k'] = 1, ['l'] = 1, ['m'] = 1, ['n'] = 1,
    ['o'] = 1, ['p'] = 1, ['q'] = 1, ['r'] = 1, ['s'] = 1, ['t'] = 1, ['u'] = 1,
    ['v'] = 1, ['w'] = 1, ['x'] = 1, ['y'] = 1, ['z'] = 1,
    ['0'] = 1, ['1'] = 1, ['2'] = 1, ['3'] = 1, ['4'] = 1, ['5'] = 1, ['6'] = 1, ['7'] = 1, ['8'] = 1, ['9'] = 1,
    ['!'] = 1, ['#'] = 1, ['$'] = 1, ['%'] = 1, ['&'] = 1, ['\''] = 1, ['*'] = 1,
    ['+'] = 1, ['-'] = 1, ['.'] = 1, ['^'] = 1, ['_'] = 1, ['`'] = 1, ['|'] = 1, ['~'] = 1
};

/************************************ Forwards *********************************/

static void freeWebFields(Web *web, bool keepAlive);
static void initWeb(Web *web, WebListen *listen, RSocket *sock, RBuf *rx, int close);
static int handleRequest(Web *web);
static bool matchFrom(Web *web, cchar *from);
static int parseHeaders(Web *web, ssize headerSize);
static void parseMethod(Web *web);
static int processBody(Web *web);
static void processOptions(Web *web);
static void processRequests(Web *web);
static void processQuery(Web *web);
static int redirectRequest(Web *web);
static void resetWeb(Web *web);
static bool routeRequest(Web *web);
static int serveRequest(Web *web);
static bool validateRequest(Web *web);
static int webActionHandler(Web *web);

/************************************* Code ***********************************/
/*
    Allocate a new web connection. This is called by the socket listener when a new connection is accepted.
 */
PUBLIC int webAlloc(WebListen *listen, RSocket *sock)
{
    Web     *web;
    WebHost *host;

    assert(!rIsMain());

    host = listen->host;

    if (host->connections >= host->maxConnections) {
        rTrace("web", "Too many connections %d/%d", (int) host->connections, (int) host->maxConnections);
        rFreeSocket(sock);
        return R_ERR_WONT_FIT;
    }
    if ((web = rAllocType(Web)) == 0) {
        return R_ERR_MEMORY;
    }
    host->connections++;
    web->conn = conn++;
    initWeb(web, listen, sock, 0, 0);
    rAddItem(host->webs, web);

    if (host->flags & WEB_SHOW_REQ_HEADERS) {
        rLog("raw", "web", "Connect: %s\n", listen->endpoint);
    }
    webHook(web, WEB_HOOK_CONNECT);

    processRequests(web);

    if (host->flags & WEB_SHOW_REQ_HEADERS) {
        rLog("raw", "web", "Disconnect: %s\n", listen->endpoint);
    }
    webHook(web, WEB_HOOK_DISCONNECT);
    webFree(web);

    host->connections--;
    return 0;
}

PUBLIC void webFree(Web *web)
{
    rRemoveItem(web->host->webs, web);
    freeWebFields(web, 0);
    rFree(web);
}

PUBLIC void webClose(Web *web)
{
    if (web) {
        web->close = 1;
    }
}

static void initWeb(Web *web, WebListen *listen, RSocket *sock, RBuf *rx, int close)
{
    web->listen = listen;
    web->host = listen->host;
    web->sock = sock;
    web->fiber = rGetFiber();

    web->buffer = 0;
    web->body = 0;
    web->error = 0;
    web->finalized = 0;
    web->rx = rx ? rx : rAllocBuf(ME_BUFSIZE);
    web->rxHeaders = rAllocBuf(ME_BUFSIZE);
    web->status = 200;
    web->signature = -1;
    web->rxRemaining = WEB_UNLIMITED;
    web->txRemaining = WEB_UNLIMITED;
    web->txLen = -1;
    web->rxLen = -1;
    web->close = close;
}

static void freeWebFields(Web *web, bool keepAlive)
{
    RSocket   *sock;
    WebListen *listen;
    RBuf      *rx;
    int64     conn;
    int       close;

    if (keepAlive) {
        close = web->close;
        conn = web->conn;
        listen = web->listen;
        rx = web->rx;
        sock = web->sock;
    } else {
        rFreeBuf(web->rx);
    }
    //  Will zero below via memset
    rFreeBuf(web->body);
    rFreeBuf(web->buffer);
    rFree(web->cookie);
    rFree(web->error);
    rFree(web->path);
    rFree(web->redirect);
    rFree(web->securityToken);
    // rFreeBuf(web->trace);
    rFreeBuf(web->rxHeaders);
    rFreeHash(web->txHeaders);
    jsonFree(web->qvars);
    jsonFree(web->vars);
    webFreeUpload(web);

#if ME_COM_WEBSOCKETS
    if (web->webSocket) {
        webSocketFree(web->webSocket);
    }
#endif
    memset(web, 0, sizeof(Web));

    if (keepAlive) {
        web->listen = listen;
        web->rx = rx;
        web->sock = sock;
        web->close = close;
        web->conn = conn;
    }
}

static void resetWeb(Web *web)
{
    if (web->close) return;

    if (web->rxRemaining > 0) {
        if (webConsumeInput(web) < 0) {
            //  Cannot read full body so close connection
            web->close = 1;
            return;
        }
    }
    freeWebFields(web, 1);
    initWeb(web, web->listen, web->sock, web->rx, web->close);
    web->reuse++;
}

/*
    Process requests on a single socket. This implements Keep-Alive and pipelined requests.
 */
static void processRequests(Web *web)
{
    while (!web->close) {
        if (serveRequest(web) < 0) {
            break;
        }
        resetWeb(web);
    }
}

/*
    Serve a request. This routine blocks the current fiber while waiting for I/O.
 */
static int serveRequest(Web *web)
{
    ssize size;

    web->started = rGetTicks();
    web->deadline = rGetTimeouts() ? web->started + web->host->parseTimeout : 0;

    /*
        Read until we have all the headers up to the limit
     */
    if ((size = webBufferUntil(web, "\r\n\r\n", web->host->maxHeader, 0)) < 0) {
        return R_ERR_CANT_READ;
    }
    if (parseHeaders(web, size) < 0) {
        return R_ERR_CANT_READ;
    }
    webAddStandardHeaders(web);
    webHook(web, WEB_HOOK_START);

    if (handleRequest(web) < 0) {
        return R_ERR_CANT_COMPLETE;
    }
    webHook(web, WEB_HOOK_END);
    return 0;
}

/*
    Handle one request. This process includes:
        redirections, authorizing the request, uploads, request body and finally
        invoking the required action or file handler.
 */
static int handleRequest(Web *web)
{
    WebRoute *route;
    cchar    *handler;

    if (web->error) {
        return 0;
    }
    if (redirectRequest(web)) {
        //  Protocol and site level redirections handled
        return 0;
    }
    if (!routeRequest(web)) {
        return 0;
    }
    route = web->route;
    handler = route->handler;

    if (web->options && route->methods) {
        processOptions(web);
        return 0;
    }
    if (web->uploads && webProcessUpload(web) < 0) {
        return 0;
    }
    if (web->query) {
        processQuery(web);
    }
#if ME_COM_WEBSOCKETS
    if (scaselessmatch(web->upgrade, "websocket")) {
        if (webUpgradeSocket(web) < 0) {
            return R_ERR_CANT_COMPLETE;
        }
    }
#endif
    if (webReadBody(web) < 0) {
        return R_ERR_CANT_COMPLETE;
    }
    webUpdateDeadline(web);

    if (!validateRequest(web)) {
        return R_ERR_BAD_REQUEST;
    }

    /*
        Request ready to run, allow any request modification or running a custom handler
     */
    webHook(web, WEB_HOOK_RUN);
    if (web->error) {
        // Return zero as a valid response has been generated
        return 0;
    }
    if (web->route->xsrf) {
        if (web->get) {
            /*
                This send the XSRF token to the client. It will generate a new XSRF token if there is
                not one already in the session state.
             */
            webAddSecurityToken(web, 0);
        } else if (!web->options && !web->head && !web->trace) {
            if (!webCheckSecurityToken(web)) {
                webError(web, 400, "Invalid XSRF token");
                return R_ERR_BAD_REQUEST;
            }
        }
    }

    /*
        Run standard handlers: action and file
     */
    if (handler[0] == 'a' && smatch(handler, "action")) {
        return webActionHandler(web);

    } else if (handler[0] == 'f' && smatch(handler, "file")) {
        return webFileHandler(web);
    }
    return webError(web, 404, "No handler to process request");
}


/*
    Validate the request against a signature from the signatures.json5 file.
    This converts the URL path into a JSON property path by changing "/" to ".".
    Return true if the request is valid. If not, we return an error code and the connection is closed.
 */
static bool validateRequest(Web *web)
{
    WebHost *host;
    char    path[WEB_MAX_SIG];
    ssize   len, urlLen;
    char    *cp;

    assert(web);
    host = web->host;

    len = slen(web->route->match);
    urlLen = slen(web->url);
    if (urlLen < len) {
        return 0;
    }
    sncopy(path, sizeof(path), &web->url[len], urlLen - len);
    for (cp = path; cp < &path[sizeof(path)] && *cp; cp++) {
        if (*cp == '/') *cp = '.';
    }
    if (host->signatures) {
        web->signature = jsonGetId(host->signatures, 0, path);
        if (web->route->validate) {
            return webValidateRequest(web, path);
        }
    }
    return 1;
}

static int webActionHandler(Web *web)
{
    WebAction *action;
    int       next;

    for (ITERATE_ITEMS(web->host->actions, action, next)) {
        if (sstarts(web->path, action->match)) {
            if (!webCan(web, action->role)) {
                webError(web, 401, "Authorization Denied.");
                return 0;
            }
            webHook(web, WEB_HOOK_ACTION);
            (action->fn)(web);
            return 0;
        }
    }
    return webError(web, 404, "No action to handle request");
}

/*
    Route the request. This matches the request URL with route URL prefixes.
    It also authorizes the request by checking the authenticated user role vs the routes required role.
    Return true if the request was routed successfully.
 */
static bool routeRequest(Web *web)
{
    WebRoute *route;
    char     *path;
    bool     match;
    int      next;

    for (ITERATE_ITEMS(web->host->routes, route, next)) {
        if (route->exact) {
            match = smatch(web->path, route->match);
        } else {
            match = sstarts(web->path, route->match);
        }
        if (match) {
            if (!rLookupName(route->methods, web->method)) {
                webError(web, 405, "Unsupported method.");
                return 0;
            }
            web->route = route;
            if (route->redirect) {
                webRedirect(web, 302, route->redirect);

            } else if (route->role && !web->options) {
                if (!webAuthenticate(web) || !webCan(web, route->role)) {
                    if (!smatch(route->role, "public")) {
                        webError(web, 401, "Access Denied. User not logged in or insufficient privilege.");
                        return 0;
                    }
                }
            }
            if (route->trim && sstarts(web->path, route->trim)) {
                path = sclone(&web->path[slen(route->trim)]);
                rFree(web->path);
                web->path = path;
            }
            return 1;
        }
    }
    rInfo("web", "Cannot find route to serve request %s", web->path);
    webHook(web, WEB_HOOK_NOT_FOUND);

    if (!web->error) {
        webWriteResponse(web, 404, "No matching route");
    }
    return 0;
}

/*
    Apply top level redirections. This is used to redirect to https and site redirections.
 */
static int redirectRequest(Web *web)
{
    WebRedirect *redirect;
    int         next;

    for (ITERATE_ITEMS(web->host->redirects, redirect, next)) {
        if (matchFrom(web, redirect->from)) {
            webRedirect(web, redirect->status, redirect->to);
            return 1;
        }
    }
    return 0;
}

static bool matchFrom(Web *web, cchar *from)
{
    char  *buf, ip[256];
    cchar *host, *path, *query, *hash, *scheme;
    bool  rc;
    int   port, portNum;

    if ((buf = webParseUrl(from, &scheme, &host, &port, &path, &query, &hash)) == 0) {
        webWriteResponse(web, 404, "Cannot parse redirection target");
        return 0;
    }
    rc = 1;
    if (scheme && !smatch(web->scheme, scheme)) {
        rc = 0;
    } else if (host || port) {
        rGetSocketAddr(web->sock, ip, sizeof(ip), &portNum);
        if (host && (!smatch(web->host->name, host) && !smatch(ip, host))) {
            rc = 0;
        } else if (port && port != portNum) {
            rc = 0;
        }
    }
    if (path && (slen(web->path) < 2 || !smatch(&web->path[1], path))) {
        //  Path does not contain leading "/"
        rc = 0;
    } else if (query && !smatch(web->query, query)) {
        rc = 0;
    } else if (hash && !smatch(web->hash, hash)) {
        rc = 0;
    }
    rFree(buf);
    return rc;
}

/*
    Parse the request headers
 */
static int parseHeaders(Web *web, ssize headerSize)
{
    RBuf *buf;
    char *end, *tok;

    buf = web->rx;
    if (headerSize <= 10) {
        return webNetError(web, "Bad request header");
    }
    end = buf->start + headerSize;
    end[-2] = '\0';
    rPutBlockToBuf(web->rxHeaders, buf->start, headerSize - 2);
    rAdjustBufStart(buf, end - buf->start);

    if (web->host->flags & WEB_SHOW_REQ_HEADERS) {
        rLog("raw", "web", "Request <<<<\n\n%s\n", web->rxHeaders->start);
    }
    web->method = supper(stok(web->rxHeaders->start, " \t", &tok));
    parseMethod(web);

    web->url = stok(tok, " \t", &tok);
    web->protocol = supper(stok(tok, "\r", &tok));
    web->scheme = rIsSocketSecure(web->sock) ? "https" : "http";

    if (tok == 0) {
        return webNetError(web, "Bad request header");
    }
    rAdjustBufStart(web->rxHeaders, tok - web->rxHeaders->start + 1);
    rAddNullToBuf(web->rxHeaders);

    /*
        Only support HTTP/1.0 without keep alive - all clients should be supporting HTTP/1.1
     */
    if (smatch(web->protocol, "HTTP/1.0")) {
        web->http10 = 1;
        web->close = 1;
    }
    if (!webParseHeadersBlock(web, web->rxHeaders->start, rGetBufLength(web->rxHeaders), 0)) {
        return R_ERR_BAD_REQUEST;
    }
    if (webValidateUrl(web) < 0) {
        return R_ERR_BAD_REQUEST;
    }
    webUpdateDeadline(web);
    return 0;
}

static void parseMethod(Web *web)
{
    cchar *method;

    method = web->method;

    switch (method[0]) {
    case 'D':
        if (strcmp(method, "DELETE") == 0) {
            web->del = 1;
        }
        break;

    case 'G':
        if (strcmp(method, "GET") == 0) {
            web->get = 1;
        }
        break;

    case 'H':
        if (strcmp(method, "HEAD") == 0) {
            web->head = 1;
        }
        break;

    case 'O':
        if (strcmp(method, "OPTIONS") == 0) {
            web->options = 1;
        }
        break;

    case 'P':
        if (strcmp(method, "POST") == 0) {
            web->post = 1;

        } else if (strcmp(method, "PUT") == 0) {
            web->put = 1;
        }
        break;

    case 'T':
        if (strcmp(method, "TRACE") == 0) {
            web->trace = 1;
        }
        break;
    }
}

/*
    Parse a headers block. Used here and by file upload.
 */
PUBLIC bool webParseHeadersBlock(Web *web, char *headers, ssize headersSize, bool upload)
{
    struct tm tm;
    cchar     *end;
    char      c, *cp, *endKey, *key, *prior, *t, *value;
    uchar     uc;
    bool      hasCL = 0, hasTE = 0;

    if (!headers || *headers == '\0') {
        return 1;
    }
    end = &headers[headersSize];

    for (cp = headers; cp < end; ) {
        key = cp;
        if ((cp = strchr(cp, ':')) == 0) {
            webNetError(web, "Bad headers");
            return 0;
        }
        endKey = cp;
        *cp++ = '\0';
        while (*cp && isWhite(*cp)) {
            cp++;
        }
        value = cp;
        while (*cp && *cp != '\r') {
            //  Only permit strict \r\n header terminator
            if (*cp == '\n') {
                webNetError(web, "Bad headers");
                return 0;
            }
            cp++;
        }
        if (*cp != '\r') {
            webNetError(web, "Bad headers");
            return 0;
        }
        *cp++ = '\0';
        if (*cp != '\n') {
            webNetError(web, "Bad headers");
            return 0;
        }
        *cp++ = '\0';

        // Trim white space from value
        for (t = cp - 2; t >= value; t--) {
            if (isWhite(*t)) {
                *t = '\0';
            } else {
                break;
            }
        }
        /* 
            From here, we've ensured the key and value are valid and do not contain any whitespace.
            This prevents CRLF injection attacks in header values.
        */

        //  Validate header name
        for (t = key; t < endKey; t++) {
            uc = (uchar) *t;
            if (uc >= sizeof(validHeaderChars) || !validHeaderChars[uc]) {
                return 0;
            }
        }
        c = tolower(*key);

        if (upload && c != 'c' && (
                !scaselessmatch(key, "content-disposition") &&
                !scaselessmatch(key, "content-type"))) {
            webNetError(web, "Bad upload headers");
            return 0;
        }
        if (c == 'c') {
            if (scaselessmatch(key, "content-disposition")) {
                web->contentDisposition = value;

            } else if (scaselessmatch(key, "content-type")) {
                web->contentType = value;
                if (scontains(value, "multipart/form-data")) {
                    if (webInitUpload(web) < 0) {
                        return 0;
                    }

                } else if (smatch(value, "application/x-www-form-urlencoded")) {
                    web->formBody = 1;

                } else if (smatch(value, "application/json")) {
                    web->jsonBody = 1;
                }

            } else if (scaselessmatch(key, "connection")) {
                if (scaselessmatch(value, "close")) {
                    web->close = 1;
                }
            } else if (scaselessmatch(key, "content-length")) {
                hasCL = 1;
                web->rxLen = web->rxRemaining = stoi(value);
                if (web->rxLen < 0 || web->rxLen > web->host->maxBody) {
                    webNetError(web, "Bad Content-Length");
                    return 0;
                }

            } else if (scaselessmatch(key, "cookie")) {
                if (web->cookie) {
                    prior = web->cookie;
                    web->cookie = sjoin(prior, "; ", value, NULL);
                    rFree(prior);
                } else {
                    web->cookie = sclone(value);
                }
            }

        } else if (c == 'i' && strcmp(key, "if-modified-since") == 0) {
#if !defined(ESP32)
            if (strptime(value, "%a, %d %b %Y %H:%M:%S", &tm) != NULL) {
                web->since = timegm(&tm);
            }
#endif
        } else if (c == 'l' && scaselessmatch(key, "last-event-id")) {
            web->lastEventId = stoi(value);

        } else if (c == 'o' && scaselessmatch(key, "origin")) {
            web->origin = value;

        } else if (c == 't' && scaselessmatch(key, "transfer-encoding")) {
            if (scaselessmatch(value, "chunked")) {
                hasTE = 1;
                web->chunked = WEB_CHUNK_START;
            }
        } else if (c == 'u' && scaselessmatch(key, "upgrade")) {
            web->upgrade = value;
        }
    }
    if (hasCL && hasTE) {
        webNetError(web, "Cannot have both Content-Length and Transfer-Encoding");
        return 0;
    }
    if (!web->chunked && !web->uploads && web->rxLen < 0) {
        web->rxRemaining = 0;
    }
    return 1;
}

/*
    Headers have been tokenized with a null replacing the ":" and "\r\n"
 */
PUBLIC cchar *webGetHeader(Web *web, cchar *name)
{
    cchar *cp, *end, *start;
    cchar *value;

    start = rGetBufStart(web->rxHeaders);
    end = rGetBufEnd(web->rxHeaders);
    value = 0;

    for (cp = start; cp < end; cp++) {
        if (scaselessmatch(cp, name)) {
            cp += slen(name) + 1;
            while (isWhite(*cp)) cp++;
            value = cp;
            break;
        }
        cp += slen(cp) + 1;
        if (cp < end && *cp) {
            cp += slen(cp) + 1;
        }
    }
    return value;
}

PUBLIC bool webGetNextHeader(Web *web, cchar **pkey, cchar **pvalue)
{
    cchar *cp, *start;

    assert(pkey);
    assert(pvalue);

    if (!pkey || !pvalue) {
        return 0;
    }
    if (*pvalue) {
        start = *pvalue + slen(*pvalue) + 2;
    } else {
        start = rGetBufStart(web->rxHeaders);
    }
    if (start < rGetBufEnd(web->rxHeaders)) {
        *pkey = start;
        cp = start + slen(start) + 1;
        while (isWhite(*cp)) cp++;
        *pvalue = cp;
        return 1;
    }
    return 0;
}

/*
    Read body data from the rx buffer into the body buffer
    Not called for streamed requests
 */
PUBLIC int webReadBody(Web *web)
{
    WebRoute *route;
    RBuf     *buf;
    ssize    nbytes;

    route = web->route;
    if ((route && route->stream) || web->webSocket || (web->rxRemaining <= 0 && !web->chunked)) {
        return 0;
    }
    if (!web->body) {
        web->body = rAllocBuf(ME_BUFSIZE);
    }
    buf = web->body;
    do {
        rReserveBufSpace(buf, ME_BUFSIZE);
        nbytes = webRead(web, rGetBufEnd(buf), rGetBufSpace(buf));
        if (nbytes < 0) {
            return R_ERR_CANT_READ;
        }
        rAdjustBufEnd(buf, nbytes);
        if (rGetBufLength(buf) > web->host->maxBody) {
            webNetError(web, "Request is too big");
            return R_ERR_CANT_READ;
        }
    } while (nbytes > 0 && web->rxRemaining > 0);
    rAddNullToBuf(buf);

    if (processBody(web) < 0) {
        //  Continue
        return 0;
    }
    return 0;
}

/*
    Process the request body and parse JSON, url-encoded forms and query vars.
 */
static int processBody(Web *web)
{
    /*
        SECURITY Acceptable: - This logging is only enabled for testing and development,
        and is an acceptable risk to use getenv here to modify log level
     */
    if (web->host->flags & WEB_SHOW_REQ_BODY && rGetBufLength(web->body)) {
        rLog("raw", "web", "Request Body <<<<\n\n%s\n\n", rBufToString(web->body));
    }
    if (web->jsonBody) {
        if ((web->vars = webParseJson(web)) == 0) {
            return webError(web, 400, "JSON body is malformed");
        }
    } else if (web->formBody) {
        web->vars = jsonAlloc();
        webParseForm(web);
    }
    return 0;
}

static void processQuery(Web *web)
{
    web->qvars = jsonAlloc();
    webParseQuery(web);
}

static void processOptions(Web *web)
{
    RList *list;
    RName *np;

    list = rAllocList(0, 0);
    for (ITERATE_NAMES(web->route->methods, np)) {
        rAddItem(list, np->name);
    }
    rSortList(list, NULL, NULL);
    webAddHeaderDynamicString(web, "Access-Control-Allow-Methods", rListToString(list, ","));
    rFreeList(list);
    webWriteResponse(web, 200, NULL);
}

PUBLIC int webHook(Web *web, int event)
{
    if (web->host->hook) {
        return (web->host->hook)(web, event);
    }
    return 0;
}

/**
    Extend the timeout for the request by updating the deadline.
    DEPRECATED: Use webUpdateDeadline() instead.
 */
PUBLIC void webExtendTimeout(Web *web, Ticks timeout)
{
    web->deadline = rGetTimeouts() ? rGetTicks() + timeout : 0;
}

/**
    Reset the deadline for the request using the inactivity timeout and request timeout.
    This is used typically when I/O activity is detected.
 */
PUBLIC void webUpdateDeadline(Web *web)
{
    if (!web->upgraded) {
        web->deadline = rGetTimeouts() ?
                        min(rGetTicks() + web->host->inactivityTimeout, web->started + web->host->requestTimeout) : 0;
    }
}

/*
    Enable response buffering
 */
PUBLIC void webBuffer(Web *web, ssize size)
{
    if (size <= 0) {
        size = ME_BUFSIZE;
    }
    size = max(size, web->host->maxBuffer);
    if (web->buffer) {
        if (rGetBufSize(web->buffer) < size) {
            rGrowBuf(web->buffer, size);
        }
    } else {
        web->buffer = rAllocBuf(size);
    }
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file ../../../src/io.c ************/

/*
    io.c - I/O for the web server

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



/************************************ Forwards *********************************/

static char *findPattern(RBuf *buf, cchar *pattern);
static RHash *getTxHeaders(Web *web);
static bool isprintable(cchar *s, ssize len);
static ssize readBlock(Web *web, char *buf, ssize bufsize);
static ssize readChunk(Web *web, char *buf, ssize bufsize);
static int writeChunkDivider(Web *web, ssize bufsize);

/************************************* Code ***********************************/
/*
    Read request body data into a buffer and return the number of bytes read.
    The web->rxRemaining indicates the number of bytes yet to read.
    This reads through the web->rx low-level buffer.
    This will block the current fiber until some data is read.
 */
PUBLIC ssize webRead(Web *web, char *buf, ssize bufsize)
{
    ssize nbytes;

    if (web->chunked) {
        nbytes = readChunk(web, buf, bufsize);
    } else {
        bufsize = min(bufsize, web->rxRemaining);
        nbytes = readBlock(web, buf, bufsize);
    }
    if (nbytes < 0) {
        if (web->rxRemaining > 0) {
            return webNetError(web, "Cannot read from socket");
        }
        web->close = 1;
        return 0;
    }
    if (web->chunked == WEB_CHUNK_EOF) {
        web->rxRemaining = 0;
    } else {
        web->rxRemaining -= nbytes;
    }
    webUpdateDeadline(web);
    return nbytes;
}

/*
    Read a chunk using transfer-chunk encoding
 */
static ssize readChunk(Web *web, char *buf, ssize bufsize)
{
    ssize chunkSize, nbytes;
    char  cbuf[32];

    nbytes = 0;

    if (web->chunked == WEB_CHUNK_START) {
        if (webReadUntil(web, "\r\n", cbuf, sizeof(cbuf)) < 0) {
            return webNetError(web, "Bad chunk data");
        }
        cbuf[sizeof(cbuf) - 1] = '\0';
        chunkSize = (int) stoix(cbuf, NULL, 16);
        if (chunkSize < 0) {
            return webNetError(web, "Bad chunk specification");

        } else if (chunkSize) {
            web->chunkRemaining = chunkSize;
            web->chunked = WEB_CHUNK_DATA;

        } else {
            //  Zero chunk -- end of body
            if (webReadUntil(web, "\r\n", cbuf, sizeof(cbuf)) < 0) {
                return webNetError(web, "Bad chunk data");
            }
            web->chunkRemaining = 0;
            web->rxRemaining = 0;
            web->chunked = WEB_CHUNK_EOF;
        }
    }
    if (web->chunked == WEB_CHUNK_DATA) {
        if ((nbytes = readBlock(web, buf, min(bufsize, web->chunkRemaining))) < 0) {
            return webNetError(web, "Cannot read chunk data");
        }
        web->chunkRemaining -= nbytes;
        if (web->chunkRemaining <= 0) {
            web->chunked = WEB_CHUNK_START;
            web->chunkRemaining = WEB_UNLIMITED;
            if (webReadUntil(web, "\r\n", cbuf, sizeof(cbuf)) < 0) {
                return webNetError(web, "Bad chunk data");
            }
        }
    }
    return nbytes;
}

/*
    Read up to bufsize bytes from the socket into the web->rx buffer.
    Return the number of bytes read or a negative error code.
 */
PUBLIC ssize webReadSocket(Web *web, ssize bufsize)
{
    RBuf  *bp;
    ssize nbytes;

    bp = web->rx;
    rCompactBuf(bp);
    rReserveBufSpace(bp, max(ME_BUFSIZE, bufsize));

    if ((nbytes = rReadSocket(web->sock, bp->end, rGetBufSpace(bp), web->deadline)) < 0) {
        return webNetError(web, "Cannot read from socket");
    }
    rAdjustBufEnd(bp, nbytes);
    return rGetBufLength(bp);
}

/*
    Read a block data from the web->rx buffer into the user buffer.
    Return the number of bytes read or a negative error code.
    Will only block and read if the buffer is empty.
 */
static ssize readBlock(Web *web, char *buf, ssize bufsize)
{
    RBuf  *bp;
    ssize nbytes;

    if (bufsize <= 0) {
        return 0;
    }
    bp = web->rx;
    if (rGetBufLength(bp) == 0 && webReadSocket(web, bufsize) < 0) {
        return R_ERR_CANT_READ;
    }
    nbytes = min(rGetBufLength(bp), bufsize);
    if (nbytes > 0) {
        memcpy(buf, bp->start, nbytes);
        rAdjustBufStart(bp, nbytes);
    }
    return nbytes;
}

/*
    Read response data until a designated pattern is read up to a limit.
    If a user buffer is provided, data is read into it, up to the limit and the rx buffer is adjusted.
    If the user buffer is not provided, don't copy into the user buffer and don't adjust the rx buffer.
    Always return the number of read up to and including the pattern.
    If the pattern is not found inside the limit, returns negative on errors.
    Note: this routine may over-read and data will be buffered for the next read.
 */
PUBLIC ssize webReadUntil(Web *web, cchar *until, char *buf, ssize limit)
{
    RBuf  *bp;
    ssize len, nbytes;

    assert(buf);
    assert(limit);
    assert(until);

    bp = web->rx;

    if ((nbytes = webBufferUntil(web, until, limit, 0)) <= 0) {
        return nbytes;
    }
    if (buf) {
        //  Copy data into the supplied buffer
        len = min(nbytes, limit);
        memcpy(buf, bp->start, len);
        rAdjustBufStart(bp, len);
    }
    return nbytes;
}

/*
    Read until the specified pattern is seen or until the size limit.
    This may read headers and body data for this request.
    If reading headers, we may read all the body data and (WARNING) any pipelined request following.
    If chunked, we may also read a subsequent pipelined request. May call webNetError.
    Set allowShort to not error if the pattern is not found before the limit.
    Return the total number of buffered bytes up to the requested pattern.
    Return zero if pattern not found before limit or negative on errors.
 */
PUBLIC ssize webBufferUntil(Web *web, cchar *until, ssize limit, bool allowShort)
{
    RBuf  *bp;
    char  *end;
    ssize nbytes, toRead, ulen;

    assert(web);
    assert(limit >= 0);
    assert(until);

    bp = web->rx;
    rAddNullToBuf(bp);

    ulen = slen(until);

    while (findPattern(bp, until) == 0 && rGetBufLength(bp) < limit) {
        rCompactBuf(bp);
        rReserveBufSpace(bp, ME_BUFSIZE);

        toRead = rGetBufSpace(bp);
        if (limit) {
            toRead = min(toRead, limit);
        }
        if (toRead <= 0) {
            //  Pattern not found before the limit
            return 0;
        }
        if ((nbytes = rReadSocket(web->sock, bp->end, toRead, web->deadline)) < 0) {
            return R_ERR_CANT_READ;
        }
        rAdjustBufEnd(bp, nbytes);
        rAddNullToBuf(bp);

        if (rGetBufLength(bp) > web->host->maxBody) {
            return webNetError(web, "Request is too big");
        }
    }
    end = findPattern(bp, until);
    if (!end) {
        if (allowShort) {
            return 0;
        }
        return webNetError(web, "Missing request pattern boundary");
    }
    //  Return data including "until" pattern
    return &end[ulen] - bp->start;
}

/*
    Find pattern in buffer
 */
static char *findPattern(RBuf *buf, cchar *pattern)
{
    char  *cp, *endp, first;
    ssize bufLen, patLen;

    assert(buf);

    first = *pattern;
    bufLen = rGetBufLength(buf);
    patLen = slen(pattern);

    if (bufLen < patLen) {
        return 0;
    }
    endp = buf->start + (bufLen - patLen) + 1;
    for (cp = buf->start; cp < endp; cp++) {
        if ((cp = (char*) memchr(cp, first, endp - cp)) == 0) {
            return 0;
        }
        if (memcmp(cp, pattern, patLen) == 0) {
            return cp;
        }
    }
    return 0;
}

/*
    Consume remaining input to try to preserve keep-alive
 */
PUBLIC int webConsumeInput(Web *web)
{
    char  buf[ME_BUFSIZE];
    ssize nbytes;

    do {
        if ((nbytes = webRead(web, buf, sizeof(buf))) < 0) {
            return R_ERR_CANT_READ;
        }
    } while (nbytes > 0);
    return 0;
}

/*
    Write response headers
 */
PUBLIC ssize webWriteHeaders(Web *web)
{
    RName *header;
    RBuf  *buf;
    cchar *connection;
    char  date[32];
    ssize nbytes;
    int   status;

    if (web->wroteHeaders) {
        rError("web", "Headers already created");
        return 0;
    }
    if (web->writingHeaders) {
        return 0;
    }
    web->writingHeaders = 1;

    status = web->status;
    if (status == 0) {
        status = 500;
    }
    buf = rAllocBuf(1024);
    webAddHeader(web, "Date", webDate(date, time(0)));
    if (web->upgrade) {
        connection = "Upgrade";
    } else if (web->close) {
        connection = "close";
    } else {
        connection = "keep-alive";
    }
    webAddHeaderStaticString(web, "Connection", connection);

    if (!((100 <= status && status <= 199) || status == 204 || status == 304)) {
        //  Server must not emit a content length header for 1XX, 204 and 304 status
        if (web->txLen < 0) {
            webAddHeaderStaticString(web, "Transfer-Encoding", "chunked");
        } else {
            web->txRemaining = web->txLen;
            webAddHeader(web, "Content-Length", "%d", web->txLen);
        }
    }
    if (web->redirect) {
        webAddHeaderStaticString(web, "Location", web->redirect);
    }
    if (!web->mime && web->ext) {
        web->mime = rLookupName(web->host->mimeTypes, web->ext);
    }
    if (web->mime) {
        webAddHeaderStaticString(web, "Content-Type", web->mime);
    }
    if (smatch(rLookupName(web->txHeaders, "Access-Control-Allow-Origin"), "dynamic") == 0) {
        webAddAccessControlHeader(web);
    }

    /*
        Emit HTTP response line
     */
    rPutToBuf(buf, "%s %d %s\r\n", web->protocol, status, webGetStatusMsg(status));
    if (!rEmitLog("trace", "web")) {
        rTrace("web", "%s", rGetBufStart(buf));
    }

    /*
        Emit response headers
     */
    for (ITERATE_NAMES(web->txHeaders, header)) {
        rPutToBuf(buf, "%s: %s\r\n", header->name, (cchar*) header->value);
    }
    if (web->host->flags & WEB_SHOW_RESP_HEADERS) {
        rLog("raw", "web", "Response >>>>\n\n%s\n", rBufToString(buf));
    }
    if (web->txLen >= 0 || web->upgraded) {
        //  Delay adding if using transfer encoding. This optimization eliminates a write per chunk.
        rPutStringToBuf(buf, "\r\n");
    }
    if ((nbytes = webWrite(web, rGetBufStart(buf), rGetBufLength(buf))) < 0) {
        rFreeBuf(buf);
        return R_ERR_CANT_WRITE;
    }
    rFreeBuf(buf);
    web->writingHeaders = 0;
    web->wroteHeaders = 1;
    return nbytes;
}

/*
    Add headers from web.json
 */
PUBLIC void webAddStandardHeaders(Web *web)
{
    WebHost  *host;
    Json     *json;
    JsonNode *header;

    host = web->host;
    if (host->headers >= 0) {
        json = host->config;
        for (ITERATE_JSON_KEY(json, host->headers, NULL, header, id)) {
            webAddHeaderStaticString(web, header->name, header->value);
        }
    }
}

/*
    Define a response header for this request. Header should be of the form "key: value\r\n".
 */
PUBLIC void webAddHeader(Web *web, cchar *key, cchar *fmt, ...)
{
    char    *value;
    va_list ap;

    va_start(ap, fmt);
    value = sfmtv(fmt, ap);
    va_end(ap);
    webAddHeaderDynamicString(web, key, value);
}

PUBLIC void webAddHeaderDynamicString(Web *web, cchar *key, char *value)
{
    rAddDuplicateName(getTxHeaders(web), key, value, R_DYNAMIC_VALUE);
}

PUBLIC void webAddHeaderStaticString(Web *web, cchar *key, cchar *value)
{
    rAddDuplicateName(getTxHeaders(web), key, (void*) value, R_STATIC_VALUE);
}

static RHash *getTxHeaders(Web *web)
{
    RHash *headers;

    headers = web->txHeaders;
    if (!headers) {
        headers = rAllocHash(16, R_DYNAMIC_VALUE);
        web->txHeaders = headers;
    }
    return headers;
}

/*
    Add an Access-Control-Allow-Origin response header necessary for CORS requests.
 */
PUBLIC void webAddAccessControlHeader(Web *web)
{
    char  *hostname;
    cchar *origin, *schema;

    origin = web->origin;
    webAddHeaderStaticString(web, "Vary", "Origin");
    if (origin) {
        webAddHeaderStaticString(web, "Access-Control-Allow-Origin", origin);
    } else {
        hostname = webGetHostName(web);
        schema = web->sock->tls ? "https" : "http";
        webAddHeader(web, "Access-Control-Allow-Origin", "%s://%s", schema, hostname);
        rFree(hostname);
    }
}

/*
    Write body data. Caller should set bufsize to zero to signify end of body
    if the content length is not defined.
    Will write headers if not alread written. Will close the socket on socket errors.
 */
PUBLIC ssize webWrite(Web *web, cvoid *buf, ssize bufsize)
{
    ssize written;

    if (web->finalized) {
        if (buf && bufsize > 0) {
            rError("web", "Web connection already finalized");
        }
        return 0;
    }
    if (buf == NULL) {
        bufsize = 0;
    } else if (bufsize < 0) {
        bufsize = slen(buf);
    }
    if (web->buffer && !web->writingHeaders) {
        if (buf) {
            rPutBlockToBuf(web->buffer, buf, bufsize);
            return bufsize;
        }
        buf = rGetBufStart(web->buffer);
        bufsize = rGetBufLength(web->buffer);
        webSetContentLength(web, bufsize);
    }
    if (!web->wroteHeaders && webWriteHeaders(web) < 0) {
        //  Already closed
        return R_ERR_CANT_WRITE;
    }
    if (web->head && !web->writingHeaders && bufsize > 0) {
        // Non-finalizing head requests remit no body
        webUpdateDeadline(web);
        return 0;
    }
    if (writeChunkDivider(web, bufsize) < 0) {
        //  Already closed
        return R_ERR_CANT_WRITE;
    }
    if (bufsize > 0) {
        if ((written = rWriteSocket(web->sock, buf, bufsize, web->deadline)) < 0) {
            return R_ERR_CANT_WRITE;
        }
        if (web->wroteHeaders && web->host->flags & WEB_SHOW_RESP_BODY) {
            if (isprintable(buf, written)) {
                if (web->moreBody) {
                    write(rGetLogFile(), (char*) buf, (int) written);
                } else {
                    rLog("raw", "web", "Response Body >>>>\n\n%*s", (int) written, (char*) buf);
                    web->moreBody = 1;
                }
            }
        }
        if (web->wroteHeaders) {
            web->txRemaining -= written;
        }
    } else {
        written = 0;
    }
    webUpdateDeadline(web);
    return written;
}

/*
    Finalize normal output for this request. This will ensure headers are written and
    will finalize transfer-encoding output. For WebSockets this must be called after the
    handshake is complete and before WebSocket processing begins.
 */
PUBLIC ssize webFinalize(Web *web)
{
    ssize nbytes;

    if (web->finalized) {
        return 0;
    }
    nbytes = webWrite(web, 0, 0);
    web->finalized = 1;
    return nbytes;
}

/*
    Write a formatted string
 */
PUBLIC ssize webWriteFmt(Web *web, cchar *fmt, ...)
{
    va_list ap;
    char    *buf;
    ssize   r;

    va_start(ap, fmt);
    buf = sfmtv(fmt, ap);
    va_end(ap);
    r = webWrite(web, buf, slen(buf));
    rFree(buf);
    return r;
}

PUBLIC ssize webWriteJson(Web *web, const Json *json)
{
    char  *str;
    ssize rc;

    if ((str = jsonToString(json, 0, NULL, JSON_JSON)) != 0) {
        rc = webWrite(web, str, -1);
        rFree(str);
        return rc;
    }
    return 0;
}

/*
    Write a transfer-chunk encoded divider if required
 */
static int writeChunkDivider(Web *web, ssize size)
{
    char chunk[24];

    if (web->txLen >= 0 || !web->wroteHeaders || web->upgraded) {
        return 0;
    }
    if (size == 0) {
        scopy(chunk, sizeof(chunk), "\r\n0\r\n\r\n");
    } else {
        sfmtbuf(chunk, sizeof(chunk), "\r\n%zx\r\n", size);
    }
    if (rWriteSocket(web->sock, chunk, slen(chunk), web->deadline) < 0) {
        return webNetError(web, "Cannot write to socket");
    }
    return 0;
}

/*
    Set the HTTP response status. This will be emitted in the HTTP response line.
 */
PUBLIC void webSetStatus(Web *web, int status)
{
    web->status = status;
}

/*
    Emit a single response and finalize the response output.
    If the status is an error (not 200 or 401), the response will be logged to the error log.
    If status is zero, set the status to 400 and close the socket after issuing the response.
 */
PUBLIC ssize webWriteResponse(Web *web, int status, cchar *fmt, ...)
{
    va_list ap;
    char    *msg;
    ssize   rc;

    if (!fmt) {
        fmt = "";
    }
    if (!status) {
        status = 400;
        web->close = 1;
    }
    web->status = status;

    if (rIsSocketClosed(web->sock)) {
        return R_ERR_CANT_WRITE;
    }
    if (web->error) {
        msg = web->error;
    } else {
        va_start(ap, fmt);
        msg = sfmtv(fmt, ap);
        va_end(ap);
    }
    web->txLen = slen(msg);

    webAddHeaderStaticString(web, "Content-Type", "text/plain");

    if (webWriteHeaders(web) < 0) {
        rc = R_ERR_CANT_WRITE;
    } else {
        if (web->status != 204 && !web->head && web->txLen > 0) {
            (void) webWrite(web, msg, web->txLen);
        }
        rc = webFinalize(web);
    }
    if (status != 200 && status != 204 && status != 301 && status != 302 && status != 401) {
        rTrace("web", "%s", msg);
    }
    if (!web->error) {
        rFree(msg);
    }
    return rc;
}

PUBLIC ssize webWriteEvent(Web *web, int64 id, cchar *name, cchar *fmt, ...)
{
    va_list ap;
    char    *buf;
    ssize   nbytes;

    if (id <= 0) {
        id = ++web->lastEventId;
    }
    va_start(ap, fmt);
    buf = sfmtv(fmt, ap);
    va_end(ap);

    if (!web->wroteHeaders) {
        webAddHeaderStaticString(web, "Content-Type", "text/event-stream");
        if (webWriteHeaders(web) < 0) {
            return R_ERR_CANT_WRITE;
        }
    }
    nbytes = webWriteFmt(web, "id: %ld\nevent: %s\ndata: %s\n\n", id, name, buf);
    rFree(buf);
    return nbytes;
}

/*
    Set the response content length.
 */
PUBLIC void webSetContentLength(Web *web, ssize len)
{
    if (len >= 0) {
        web->txLen = len;
    } else {
        webError(web, 500, "Invalid content length");
    }
}

/*
    Get the hostname of the endpoint serving the request. This uses any defined canonical host name
    defined in the web.json, or the listening endpoint name. If all else fails, use the socket address.
 */
PUBLIC char *webGetHostName(Web *web)
{
    char *hostname, ipbuf[40];
    int  port;

    if (web->host->name) {
        //  Canonical host name
        hostname = sclone(web->host->name);
    } else {
        if ((hostname = strstr(web->listen->endpoint, "://")) != 0 && *hostname != ':') {
            hostname = sclone(&hostname[3]);
        } else {
            if (rGetSocketAddr(web->sock, (char*) &ipbuf, sizeof(ipbuf), &port) < 0) {
                webError(web, 0, "Missing hostname");
                return 0;
            }
            if (smatch(ipbuf, "::1") || smatch(ipbuf, "127.0.0.1")) {
                hostname = sfmt("localhost:%d", port);
            } else if (smatch(ipbuf, "0.0.0.0") && web->host->ip) {
                hostname = sfmt("%s:%d", web->host->ip, port);
            } else {
                hostname = sfmt("%s:%d", ipbuf, port);
            }
        }
    }
    return hostname;
}

/*
    Redirect the user to another web page. Target may be null.
 */
PUBLIC void webRedirect(Web *web, int status, cchar *target)
{
    char  *currentPort, *buf, *freeHost, ip[256], pbuf[16], *uri;
    cchar *host, *path, *psep, *qsep, *hsep, *query, *hash, *scheme;
    int   port;

    /*
        Read the body to ensure that the request is complete.
     */
    (void) webReadBody(web);

    //  Note: the path, query and hash do not contain /, ? or #
    if ((buf = webParseUrl(target, &scheme, &host, &port, &path, &query, &hash)) == 0) {
        webWriteResponse(web, 404, "Cannot parse redirection target");
        return;
    }
    if (!port && !scheme && !host) {
        rGetSocketAddr(web->sock, ip, sizeof(ip), &port);
    }
    if (!host) {
        freeHost = webGetHostName(web);
        host = stok(freeHost, ":", &currentPort);
        if (!port && currentPort && smatch(web->scheme, scheme)) {
            //  Use current port if the scheme isn't changing
            port = (int) stoi(currentPort);
        }
    } else {
        freeHost = 0;
    }
    if (!scheme) {
        scheme = rIsSocketSecure(web->sock) ? "https" : "http";
    }
    if (!path) {
        //  If a path isn't supplied in the target, keep the current path, query and hash
        path = &web->path[1];
        if (!query) {
            query = web->query ? web->query : NULL;
        }
        if (!hash) {
            hash = web->hash ? web->hash : NULL;
        }
    }
    qsep = query ? "?" : "";
    hsep = hash ? "#" : "";
    query = query ? query : "";
    hash = hash ? hash : "";

    if ((port == 80 && (smatch(scheme, "http") || smatch(scheme, "ws"))) ||
        (port == 443 && (smatch(scheme, "https") || smatch(scheme, "wss")))) {
        port = 0;
    }
    if (port) {
        sitosbuf(pbuf, sizeof(pbuf), port, 10);
    } else {
        pbuf[0] = '\0';
    }
    psep = port ? ":" : "";
    uri = sfmt("%s://%s%s%s/%s%s%s%s%s", scheme, host, psep, pbuf, path, qsep, query, hsep, hash);

    rFree(web->redirect);
    web->redirect = webEncode(uri);
    web->upgrade = NULL;
    rFree(uri);

    webWriteResponse(web, status, NULL);
    rFree(freeHost);
    rFree(buf);
}

/*
    Issue a request error response.
    If status is zero, close the socket after trying to issue a response and return an error code.
    Otherwise, the socket and connection remain usable for further requests and we return zero.
 */
PUBLIC int webError(Web *web, int status, cchar *fmt, ...)
{
    va_list args;

    if (!web->error) {
        rFree(web->error);
        va_start(args, fmt);
        web->error = sfmtv(fmt, args);
        va_end(args);
    }
    webWriteResponse(web, status, NULL);
    if (status == 0) {
        web->close = 1;
    }
    webHook(web, WEB_HOOK_ERROR);
    return status == 0 ? R_ERR_CANT_COMPLETE : 0;
}

/*
    Indicate an error and immediately close the socket. This issues no response to the client.
    Use when the connection is not usable or the client is not to be trusted.
 */
PUBLIC int webNetError(Web *web, cchar *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    if (!web->error && fmt) {
        rFree(web->error);
        web->error = sfmtv(fmt, args);
        rTrace("web", "%s", web->error);
    }
    web->status = 550;
    va_end(args);
    rCloseSocket(web->sock);
    webHook(web, WEB_HOOK_ERROR);
    return R_ERR_CANT_COMPLETE;
}

static bool isprintable(cchar *s, ssize len)
{
    cchar *cp;
    int   c;

    for (cp = s; *cp && cp < &s[len]; cp++) {
        c = *cp;
        if ((c > 126) || (c < 32 && c != 10 && c != 13 && c != 9)) {
            return 0;
        }
    }
    return 1;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file ../../../src/session.c ************/

/*
    session.c - User session state control

    This implements server side request state that is identified by a request cookie.

    Sessions are created on-demand whenever a variable is set in the session state via webSetSessionVar.
    Sessions can be manually created via webCreateSession and destroyed via webDestroySession.

    XSRF tokens are created for routes that have xsrf enabled. When a GET request is made, http.c:handleRequest
    will call web AddSecurityToken to add the XSRF token to the response headers and cookies.
    When a subsequent POST|PUT|DELETE request is made, http.c:handleRequest will call webCheckSecurityToken to
    check the XSRF token.

    Clients

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



/************************************ Locals **********************************/
#if ME_WEB_SESSIONS

#define WEB_SESSION_PRUNE (60 * 1000)      /* Prune sessions every minute */

/*********************************** Forwards *********************************/

static WebSession *createSession(Web *web);
static void pruneSessions(WebHost *host);

/************************************ Locals **********************************/

PUBLIC int webInitSessions(WebHost *host)
{
    host->sessionEvent = rStartEvent((REventProc) pruneSessions, host, WEB_SESSION_PRUNE);
    return 0;
}

static WebSession *webAllocSession(Web *web, int lifespan)
{
    WebSession *sp;

    assert(web);

    if ((sp = rAllocType(WebSession)) == 0) {
        return 0;
    }
    sp->lifespan = lifespan;
    sp->expires = rGetTicks() + lifespan;
    sp->id = cryptID(32);

    if ((sp->cache = rAllocHash(0, 0)) == 0) {
        rFree(sp->id);
        rFree(sp);
        return 0;
    }
    if (rAddName(web->host->sessions, sp->id, sp, 0) == 0) {
        rFree(sp->id);
        rFree(sp);
        return 0;
    }
    return sp;
}

PUBLIC void webFreeSession(WebSession *sp)
{
    assert(sp);

    if (sp->cache) {
        rFreeHash(sp->cache);
        sp->cache = 0;
    }
    rFree(sp->id);
    rFree(sp);
}

PUBLIC void webDestroySession(Web *web)
{
    WebSession *session;

    if ((session = webGetSession(web, 0)) != 0) {
        webSetCookie(web, web->host->sessionCookie, NULL, "/", 0, 0);
        rRemoveName(web->host->sessions, session->id);
        webFreeSession(session);
        web->session = 0;
    }
}

PUBLIC WebSession *webCreateSession(Web *web)
{
    webDestroySession(web);
    web->session = createSession(web);
    return web->session;
}

/*
    Get the user session by parsing the cookie. If "create" is true, create the session if required.
 */
WebSession *webGetSession(Web *web, int create)
{
    WebSession *session;
    char       *id;

    assert(web);

    session = web->session;

    if (!session) {
        id = webParseCookie(web, web->host->sessionCookie);
        if ((session = rLookupName(web->host->sessions, id)) == 0) {
            if (create) {
                session = createSession(web);
            }
            web->session = session;
        }
        rFree(id);
    }
    if (session) {
        session->expires = rGetTicks() + session->lifespan;
    }
    return session;
}

static WebSession *createSession(Web *web)
{
    WebSession *session;
    WebHost    *host;
    int        count;

    host = web->host;

    count = rGetHashLength(host->sessions);
    if (count >= host->maxSessions) {
        rError("session", "Too many sessions %d/%lld", count, host->maxSessions);
        return 0;
    }
    if ((session = webAllocSession(web, host->sessionTimeout)) == 0) {
        return 0;
    }
    webSetCookie(web, web->host->sessionCookie, session->id, "/", 0, 0);
    return session;
}

PUBLIC char *webParseCookie(Web *web, cchar *name)
{
    char *buf, *cookie, *end, *key, *tok, *value, *vtok;

    assert(web);

    //  Limit cookie size to 8192 bytes for security
    if (web->cookie == 0 || name == 0 || *name == '\0' || slen(web->cookie) > 8192) {
        return 0;
    }
    buf = sclone(web->cookie);
    end = &buf[slen(buf)];
    value = 0;

    for (tok = buf; tok && tok < end; ) {
        cookie = stok(tok, ";", &tok);
        cookie = strim(cookie, " ", R_TRIM_START);
        key = stok(cookie, "=", &vtok);
        if (smatch(key, name)) {
            // Remove leading spaces first, then double quotes. Spaces inside double quotes preserved.
            value = sclone(strim(strim(vtok, " ", R_TRIM_BOTH), "\"", R_TRIM_BOTH));
            break;
        }
    }
    rFree(buf);
    return value;
}

/*
    Get a session variable from the session state
 */
PUBLIC cchar *webGetSessionVar(Web *web, cchar *key, cchar *defaultValue)
{
    WebSession *sp;
    cchar      *value;

    assert(web);
    assert(key && *key);

    if ((sp = webGetSession(web, 0)) != 0) {
        if ((value = rLookupName(sp->cache, key)) == 0) {
            return defaultValue;
        }
        return value;
    }
    return 0;
}

/*
    Remove a session variable from the session state
 */
PUBLIC void webRemoveSessionVar(Web *web, cchar *key)
{
    WebSession *sp;

    assert(web);
    assert(key && *key);

    if ((sp = webGetSession(web, 0)) != 0) {
        rRemoveName(sp->cache, key);
    }
}

/*
    Set a session variable in the session state
 */
PUBLIC cchar *webSetSessionVar(Web *web, cchar *key, cchar *fmt, ...)
{
    WebSession *sp;
    RName      *np;
    char       *value;
    va_list    ap;

    assert(web);
    assert(key && *key);
    assert(fmt);

    if ((sp = webGetSession(web, 1)) == 0) {
        return 0;
    }
    va_start(ap, fmt);
    value = sfmtv(fmt, ap);
    va_end(ap);

    if ((np = rAddName(sp->cache, key, (void*) value, R_DYNAMIC_VALUE)) == 0) {
        return 0;
    }
    return np->value;
}

/*
    Remove expired sessions. Timeout is set in web.json.
 */
static void pruneSessions(WebHost *host)
{
    WebSession *sp;
    Ticks      when;
    RName      *np;
    int        count, oldCount;

    when = rGetTicks();
    oldCount = rGetHashLength(host->sessions);

    for (ITERATE_NAMES(host->sessions, np)) {
        sp = (WebSession*) np->value;
        if (sp->expires <= when) {
            rRemoveName(host->sessions, sp->id);
            webFreeSession(sp);
        }
    }
    count = rGetHashLength(host->sessions);
    if (oldCount != count || count) {
        rDebug("session", "Prune %d sessions. Remaining: %d", oldCount - count, count);
    }
    host->sessionEvent = rStartEvent((REventProc) pruneSessions, host, WEB_SESSION_PRUNE);
}

/*
    Get a security token to use to mitiate CSRF threats and store it in the session state.
    This will create a security token and save it in session state.
    This will not send the token to the client. Use webAddSecurityToken to send the XSRF token to the client.
    Security tokens are expected to be sent with POST requests to verify the request is not being forged.
    Note: the Session API prevents session hijacking by pairing with the client IP
 */
PUBLIC cchar *webGetSecurityToken(Web *web, bool recreate)
{
    cchar *token;

    if (recreate) {
        rFree(web->securityToken);
        web->securityToken = 0;
    } else if (!web->securityToken) {
        // Find the existing token in the session state
        if ((token = webGetSessionVar(web, WEB_SESSION_XSRF, 0)) != 0) {
            web->securityToken = sclone(token);
        }
    }
    if (web->securityToken == 0) {
        web->securityToken = cryptID(32);
        webSetSessionVar(web, WEB_SESSION_XSRF, web->securityToken);
    }
    return web->securityToken;
}

/*
    Add the security token to an XSRF cookie and an X-XSRF-TOKEN response header
    Set recreate to true to force a recreation of the token.
    This will create a session and cause a session cookie to be set in the response.
 */
PUBLIC int webAddSecurityToken(Web *web, bool recreate)
{
    cchar *securityToken;

    securityToken = webGetSecurityToken(web, recreate);
    webAddHeader(web, WEB_XSRF_HEADER, securityToken);
    return 0;
}

/*
    Check the security token with the request. This must match the last generated token stored in the session state.
    It is expected the client will set the X-XSRF-TOKEN request header with the token.
    The user should call this function in the post action handler.
 */
PUBLIC bool webCheckSecurityToken(Web *web)
{
    cchar *requestToken, *sessionToken;

    if ((sessionToken = webGetSessionVar(web, WEB_SESSION_XSRF, 0)) == 0) {
        // No prior GET to establish a token
        webAddSecurityToken(web, 1);
        return 0;
    } else {
        requestToken = webGetHeader(web, WEB_XSRF_HEADER);
        if (!requestToken) {
            requestToken = webGetVar(web, WEB_XSRF_PARAM, 0);
            if (!requestToken) {
                rError("session", "Missing security token in request");
                webAddSecurityToken(web, 1);
                return 0;
            }
        }
        if (!requestToken || !cryptMatch(sessionToken, requestToken)) {
            /*
                Potential CSRF attack. Deny request.
             */
            rError("session", "Security token in request does not match session token");
            webAddSecurityToken(web, 1);
            return 0;
        }
    }
    return 1;
}

/*
    Get a request cookie. The web->cookie contains a list of header cookies.
    A site may submit multiple cookies separated by ";"
 */
PUBLIC char *webGetCookie(Web *web, cchar *name)
{
    char *buf, *cookie, *end, *key, *tok, *value, *vtok;

    //  Limit cookie size to 8192 bytes for security (consistent with webParseCookie)
    if (web->cookie == 0 || name == 0 || *name == '\0' || slen(web->cookie) > 8192) {
        return 0;
    }
    buf = sclone(web->cookie);
    end = &buf[slen(buf)];
    value = 0;

    for (tok = buf; tok && tok < end; ) {
        cookie = stok(tok, ";", &tok);
        cookie = strim(cookie, " ", R_TRIM_BOTH);
        key = stok(cookie, "=", &vtok);
        if (smatch(key, name)) {
            // Remove leading spaces first, then double quotes. Spaces inside double quotes preserved.
            value = sclone(strim(strim(vtok, " ", R_TRIM_BOTH), "\"", R_TRIM_BOTH));
            break;
        }
    }
    rFree(buf);
    return value;
}

/*
    Set a response cookie
 */
PUBLIC void webSetCookie(Web *web, cchar *name, cchar *value, cchar *path, Ticks lifespan, int flags)
{
    WebHost *host;
    cchar   *httpOnly, *secure, *sameSite;
    Ticks   maxAge;

    host = web->host;
    if (flags & WEB_COOKIE_OVERRIDE) {
        httpOnly = flags & WEB_COOKIE_HTTP_ONLY ? "HttpOnly; " : "";
        secure = flags & WEB_COOKIE_SECURE ? "Secure; " : "";
        sameSite = flags & WEB_COOKIE_SAME_SITE ? host->sameSite : "Lax";
    } else {
        httpOnly = host->httpOnly ? "HttpOnly; " : "";
        secure = rIsSocketSecure(web->sock) ? "Secure; " : "";
        sameSite = host->sameSite ? host->sameSite : "Lax";
    }
    if (path == 0) {
        path = "/";
    }
    maxAge = (lifespan ? lifespan: host->sessionTimeout) / TPS;
    webAddHeader(web, "Set-Cookie", "%s=%s; Max-Age=%d; path=%s; %s%sSameSite=%s",
                 name, value, maxAge, path, secure, httpOnly, sameSite);
}
#endif /* ME_WEB_SESSION */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file ../../../src/sockets.c ************/

/*
    sockets.c - WebSockets

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



#if ME_COM_WEBSOCKETS
/********************************** Forwards **********************************/

static int addHeaders(Web *web);
static int selectProtocol(Web *web, cchar *protocol);

/*********************************** Code *************************************/

PUBLIC int webSocketOpen(WebHost *host)
{
    assert(host);
    return 0;
}

PUBLIC void webSocketClose(WebHost *host)
{
    assert(host);
}

PUBLIC int webUpgradeSocket(Web *web)
{
    WebSocket *ws;
    WebHost   *host;

    assert(web);

    if (!web->host->webSocketsEnable || web->error || web->wroteHeaders || !web->get) {
        return R_ERR_BAD_STATE;
    }
    host = web->host;

    if ((ws = webSocketAlloc(web->sock, WS_SERVER)) == 0) {
        rFatal("sockets", "memory error");
        return R_ERR_MEMORY;
    }
    web->webSocket = ws;

    /*
        Select the app protocol to use from the client request set of acceptable protocols
        If no preferred protocol is defined, use the first protocol.
     */
    if (selectProtocol(web, host->webSocketsProtocol) < 0) {
        return R_ERR_BAD_ARGS;
    }
    webSocketSetPingPeriod(ws, host->webSocketsPingPeriod);
    webSocketSetValidateUTF(ws, host->webSocketsValidateUTF);
    webSocketSetLimits(ws, host->webSocketsMaxFrame, host->webSocketsMaxMessage);

    web->deadline = MAXINT64;
    web->rxRemaining = WEB_UNLIMITED;
    web->txRemaining = WEB_UNLIMITED;
    web->close = 1;
    web->upgraded = 1;

    if (addHeaders(web) < 0) {
        return R_ERR_BAD_STATE;
    }
    return 0;
}

/*
    Select the app protocol to use from the client request set of acceptable protocols
    If none defined, use the first protocol.
 */
static int selectProtocol(Web *web, cchar *protocol)
{
    WebSocket *ws;
    char      *protocols, *tok;
    cchar     *kind;
    int       count;

    ws = web->webSocket;
    protocols = sclone(webGetHeader(web, "sec-websocket-protocol"));
    if (protocols && *protocols) {
        // Just select the first matching protocol
        count = 0;
        for (kind = stok(protocols, " \t,", &tok); kind; kind = stok(NULL, " \t,", &tok)) {
            if (!protocol || smatch(protocol, kind)) {
                break;
            }
            if (++count > 10) {
                //  DOS protection
                rFree(protocols);
                return webError(web, 400, "Too many protocols");
                break;
            }
        }
        if (!kind) {
            rFree(protocols);
            return webError(web, 400, "Unsupported Sec-WebSocket-Protocol");
        }
        webSocketSelectProtocol(ws, kind);
    } else {
        //  Client did not send a protocol list.
        webSocketSelectProtocol(ws, NULL);
    }
    rFree(protocols);
    return 0;
}

static int addHeaders(Web *web)
{
    cchar *key, *protocol;
    char  keybuf[128];
    int   version;

    assert(web);

    version = (int) stoi(webGetHeader(web, "sec-websocket-version"));
    if (version < WS_VERSION) {
        webAddHeader(web, "Sec-WebSocket-Version", "%d", WS_VERSION);
        webError(web, 400, "Unsupported Sec-WebSocket-Version");
        return R_ERR_BAD_ARGS;
    }
    if ((key = webGetHeader(web, "sec-websocket-key")) == 0) {
        webError(web, 400, "Bad Sec-WebSocket-Key");
        return R_ERR_BAD_ARGS;
    }
    webSetStatus(web, 101);
    webAddHeader(web, "Connection", "Upgrade");
    webAddHeader(web, "Upgrade", "WebSocket");

    sjoinbuf(keybuf, sizeof(keybuf), key, WS_MAGIC);
    webAddHeaderDynamicString(web, "Sec-WebSocket-Accept", cryptGetSha1Base64(keybuf, -1));

    protocol = webSocketGetProtocol(web->webSocket);
    if (protocol && *protocol) {
        webAddHeaderStaticString(web, "Sec-WebSocket-Protocol", protocol);
    }
    webAddHeader(web, "X-Request-Timeout", "%lld", web->host->requestTimeout / TPS);
    webAddHeader(web, "X-Inactivity-Timeout", "%lld", web->host->inactivityTimeout / TPS);
    webFinalize(web);
    return 0;
}

PUBLIC void webAsync(Web *web, WebSocketProc callback, void *arg)
{
    webSocketAsync(web->webSocket, callback, arg, web->rx);
}

PUBLIC int webWait(Web *web)
{
    return webSocketWait(web->webSocket, web->deadline);
}
#endif /* ME_COM_WEBSOCKETS */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file ../../../src/test.c ************/

/*
    test.c - Test routines for debug mode only

    Should NEVER be included in a production build

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include "url.h"


#if ME_DEBUG
/************************************* Locals *********************************/

static void showRequestContext(Web *web, Json *json);
static void showServerContext(Web *web, Json *json);

/************************************* Code ***********************************/

static void showRequest(Web *web)
{
    Json     *json;
    JsonNode *node;
    char     *body, keybuf[80];
    cchar    *key, *value;
    ssize    len;
    bool     isPrintable;
    int      i;

    json = jsonAlloc();
    jsonSetFmt(json, 0, "url", "%s", web->url);
    jsonSetFmt(json, 0, "method", "%s", web->method);
    jsonSetFmt(json, 0, "protocol", "%s", web->protocol);
    jsonSetFmt(json, 0, "connection", "%ld", web->conn);
    jsonSetFmt(json, 0, "reuse", "%ld", web->reuse);

    /*
        Query
     */
    if (web->qvars) {
        for (ITERATE_JSON(web->qvars, NULL, node, nid)) {
            jsonSetFmt(json, 0, SFMT(keybuf, "query.%s", node->name), "%s", node->value);
        }
    }
    /*
        Http Headers
     */
    key = value = 0;
    while (webGetNextHeader(web, &key, &value)) {
        jsonSetFmt(json, 0, SFMT(keybuf, "headers.%s", key), "%s", value);
    }

    /*
        Form vars
     */
    if (web->vars) {
        jsonBlend(json, 0, "form", web->vars, 0, NULL, 0);
    }

    /*
        Upload files
     */
#if ME_WEB_UPLOAD
    if (web->uploads) {
        WebUpload *file;
        RName     *up;
        int       aid;
        for (ITERATE_NAME_DATA(web->uploads, up, file)) {
            aid = jsonSet(json, 0, "uploads[$]", NULL, JSON_OBJECT);
            jsonSetFmt(json, aid, "filename", "%s", file->filename);
            jsonSetFmt(json, aid, "clientFilename", "%s", file->clientFilename);
            jsonSetFmt(json, aid, "contentType", "%s", file->contentType);
            jsonSetFmt(json, aid, "name", "%s", file->name);
            jsonSetFmt(json, aid, "size", "%zd", file->size);
        }
    }
#endif
    /*
        Rx Body
     */
    if (web->body && rGetBufLength(web->body)) {
        len = rGetBufLength(web->body);
        body = rGetBufStart(web->body);
        jsonSetFmt(json, 0, "bodyLength", "%ld", len);
        isPrintable = 1;
        for (i = 0; i < len; i++) {
            if (!isprint((uchar) body[i]) && body[i] != '\n' && body[i] != '\r' && body[i] != '\t') {
                isPrintable = 0;
                break;
            }
        }
        if (isPrintable) {
            jsonSetFmt(json, 0, "body", "%s", rBufToString(web->body));
        }
    }
    showRequestContext(web, json);
    showServerContext(web, json);

    webAddHeaderStaticString(web, "Content-Type", "application/json");
    webWriteJson(web, json);
    jsonFree(json);
}

static void showRequestContext(Web *web, Json *json)
{
    char ipbuf[256];
    int  port;

    jsonSetFmt(json, 0, "authenticated", "%s", web->authChecked ? "authenticated" : "public");
    if (web->contentDisposition) {
        jsonSetFmt(json, 0, "contentDisposition", "%s", web->contentDisposition);
    }
    if (web->chunked) {
        jsonSetFmt(json, 0, "contentLength", "%s", "chunked");
    } else {
        jsonSetFmt(json, 0, "contentLength", "%lld", web->rxLen);
    }
    if (web->contentType) {
        jsonSetFmt(json, 0, "contentType", "%s", web->contentType);
    }
    jsonSetFmt(json, 0, "close", "%s", web->close ? "close" : "keep-alive");

    if (web->cookie) {
        jsonSetFmt(json, 0, "cookie", "%s", web->cookie);
    }
    rGetSocketAddr(web->sock, ipbuf, sizeof(ipbuf), &port);
    jsonSetFmt(json, 0, "endpoint", "%s:%d", ipbuf, port);

    if (web->error) {
        jsonSetFmt(json, 0, "error", "%s", web->error);
    }
    if (web->hash) {
        jsonSetFmt(json, 0, "hash", "%s", web->hash);
    }
    if (web->route) {
        jsonSetFmt(json, 0, "route", "%s", web->route->match);
    }
    if (web->mime) {
        jsonSetFmt(json, 0, "mimeType", "%s", web->mime);
    }
    if (web->origin) {
        jsonSetFmt(json, 0, "origin", "%s", web->origin);
    }
    if (web->role) {
        jsonSetFmt(json, 0, "role", "%s", web->role);
    }
    if (web->session) {
        jsonSetFmt(json, 0, "session", "%s", web->session ? web->session->id : "");
    }
    if (web->username) {
        jsonSetFmt(json, 0, "username", "%s", web->username);
    }
}

static void showServerContext(Web *web, Json *json)
{
    WebHost *host;

    host = web->host;
    if (host->name) {
        jsonSetFmt(json, 0, "host.name", "%s", host->name);
    }
    jsonSetFmt(json, 0, "host.documents", "%s", host->docs);
    jsonSetFmt(json, 0, "host.index", "%s", host->index);
    jsonSetFmt(json, 0, "host.sameSite", "%s", host->sameSite);
    jsonSetFmt(json, 0, "host.uploadDir", "%s", host->uploadDir);
    jsonSetFmt(json, 0, "host.inactivityTimeout", "%d", host->inactivityTimeout);
    jsonSetFmt(json, 0, "host.parseTimeout", "%d", host->parseTimeout);
    jsonSetFmt(json, 0, "host.requestTimeout", "%d", host->requestTimeout);
    jsonSetFmt(json, 0, "host.sessionTimeout", "%d", host->sessionTimeout);
    jsonSetFmt(json, 0, "host.connections", "%d", host->connections);
    jsonSetFmt(json, 0, "host.maxBody", "%lld", host->maxBody);
    jsonSetFmt(json, 0, "host.maxConnections", "%lld", host->maxConnections);
    jsonSetFmt(json, 0, "host.maxHeader", "%lld", host->maxHeader);
    jsonSetFmt(json, 0, "host.maxSessions", "%lld", host->maxSessions);
    jsonSetFmt(json, 0, "host.maxUpload", "%lld", host->maxUpload);
}

/*
    SSE test
 */
static void eventAction(Web *web)
{
    int i;

    for (i = 0; i < 100; i++) {
        webWriteEvent(web, 0, "test", "Event %d", i);
    }
    webFinalize(web);
}

static void formAction(Web *web)
{
    char *name, *address;

    webAddHeaderStaticString(web, "Cache-Control", "no-cache");
    name = webEscapeHtml(webGetVar(web, "name", ""));
    address = webEscapeHtml(webGetVar(web, "address", ""));

    webWriteFmt(web, "<html><head><title>form.esp</title></head>\n");
    webWriteFmt(web, "<body><form name='details' method='post' action='form'>\n");
    webWriteFmt(web, "Name <input type='text' name='name' value='%s'>\n", name);
    webWriteFmt(web, "Address <input type='text' name='address' value='%s'>\n", address);
    webWriteFmt(web, "<input type='submit' name='submit' value='OK'></form>\n\n");
    webWriteFmt(web, "<h3>Request Details</h3>\n\n");
    webWriteFmt(web, "<pre>\n");
    showRequest(web);
    webWriteFmt(web, "</pre>\n</body>\n</html>\n");
    webFinalize(web);
    rFree(name);
    rFree(address);
}

static void errorAction(Web *web)
{
    webWriteResponse(web, URL_CODE_OK, "error\n");
}

static void bulkOutput(Web *web)
{
    ssize count, i;

    count = stoi(webGetVar(web, "count", "100"));
    for (i = 0; i < count; i++) {
        webWriteFmt(web, "Hello World %010d\n", i);
    }
    webFinalize(web);
}

static void showAction(Web *web)
{
    showRequest(web);
    webFinalize(web);
}

static void successAction(Web *web)
{
    webWriteResponse(web, URL_CODE_OK, "success\n");
}

static void bufferAction(Web *web)
{
    webBuffer(web, 64 * 1024);
    webWriteFmt(web, "Hello World %d\n", 1);
    webWriteFmt(web, "Hello World %d\n", 2);
    webWriteFmt(web, "Hello World %d\n", 3);
    webWriteFmt(web, "Hello World %d\n", 4);
    webWriteFmt(web, "Hello World %d\n", 5);
    webWriteFmt(web, "Hello World %d\n", 6);
    webWriteFmt(web, "Hello World %d\n", 7);
    webFinalize(web);
}

static void sigAction(Web *web)
{
    // Pretend to be authenticated with "user" role
    web->role = "user";
    web->authChecked = 1;
    web->username = "user";

    if (web->vars) {
        webWriteValidatedJson(web, web->vars, NULL);
    } else {
        webWriteValidatedData(web, rBufToString(web->body), NULL);
    }
    webFinalize(web);
}

#if ME_WEB_UPLOAD
static void uploadAction(Web *web)
{
    WebUpload *file;
    RName     *up;
    char      *path;

    showRequest(web);
    if (web->uploads) {
        for (ITERATE_NAME_DATA(web->uploads, up, file)) {
            path = rJoinFile("/tmp", file->clientFilename);
            if (rCopyFile(file->filename, path, 0600) < 0) {
                webError(web, 500, "Cant open output upload filename");
                rFree(path);
                break;
            }
            rFree(path);
        }
    }
    webFinalize(web);
}
#endif

static void sessionAction(Web *web)
{
    cchar *sessionToken;
    char  *token;

    if (smatch(web->path, "/test/session/create")) {
        /*
            Set a token in the session state and return it to the client.
            It will send it back in the query string to the /check action below.
         */
        token = cryptID(32);
        webSetSessionVar(web, "token", token);
        webWriteFmt(web, "%s", token);
        rFree(token);

    } else if (smatch(web->path, "/test/session/check")) {
        /*
            Check the session token matches the query token
         */
        sessionToken = webGetSessionVar(web, "token", 0);
        if (smatch(web->query, sessionToken)) {
            webWriteFmt(web, "success");
        } else {
            webWriteFmt(web, "token mismatch");
        }

    } else if (smatch(web->path, "/test/session/form.html")) {
        /*
           if (web->get) {
            // Add an XSRF token to the response in the headers and cookies
            webAddSecurityToken(web, 1);

           } else
           if (web->post) {
            if (webCheckSecurityToken(web)) {
                webWriteFmt(web, "success");
            } else {
                webWriteFmt(web, "Security token matches");
            }
           }
         */
        webWriteFmt(web, "success");
    }
    webFinalize(web);
}

/*
    This action is invoked for the GET and POST requests to the /test/xsrf/ route.
    The core engine will add an XSRF token in response to the GET request and will validate it
    in subsequent POST requests. We don't need to do anything here.
 */
static void xsrfAction(Web *web)
{
    webWriteFmt(web, "success");
    webFinalize(web);
}

/*
    Read a streamed rx body
 */
static void streamAction(Web *web)
{
    char  buf[ME_BUFSIZE];
    ssize nbytes, total;

    total = 0;
    do {
        if ((nbytes = webRead(web, buf, sizeof(buf))) > 0) {
            total += nbytes;
        }
    } while (nbytes > 0);
    webWriteFmt(web, "{length: %d}", total);
    webFinalize(web);
}

#if ME_COM_WEBSOCKETS
/*
    Echo back user provided message.
    On connected, buf will be null
 */
static void onEvent(WebSocket *ws, int event, cchar *buf, ssize len, Web *web)
{
    if (event == WS_EVENT_MESSAGE) {
        // rTrace("test", "Echoing: %ld bytes", len);
        webSocketSend(ws, "%s", buf);
    }
}

static void webSocketAction(Web *web)
{
    if (!web->upgrade) {
        webError(web, 400, "Connection not upgraded to WebSocket");
        return;
    }
    webAsync(web, (WebSocketProc) onEvent, web);
    if (webWait(web) < 0) {
        webError(web, 400, "Cannot wait for WebSocket");
        return;
    }
    rDebug("test", "WebSocket closed");
}
#endif

PUBLIC void webTestInit(WebHost *host, cchar *prefix)
{
    char url[128];

    webAddAction(host, SFMT(url, "%s/event", prefix), eventAction, NULL);
    webAddAction(host, SFMT(url, "%s/form", prefix), formAction, NULL);
    webAddAction(host, SFMT(url, "%s/bulk", prefix), bulkOutput, NULL);
    webAddAction(host, SFMT(url, "%s/error", prefix), errorAction, NULL);
    webAddAction(host, SFMT(url, "%s/success", prefix), successAction, NULL);
    webAddAction(host, SFMT(url, "%s/show", prefix), showAction, NULL);
    webAddAction(host, SFMT(url, "%s/stream", prefix), streamAction, NULL);
#if ME_WEB_UPLOAD
    webAddAction(host, SFMT(url, "%s/upload", prefix), uploadAction, NULL);
#endif
#if ME_COM_WEBSOCKETS
    webAddAction(host, SFMT(url, "%s/ws", prefix), webSocketAction, NULL);
#endif
    webAddAction(host, SFMT(url, "%s/session", prefix), sessionAction, NULL);
    webAddAction(host, SFMT(url, "%s/xsrf", prefix), xsrfAction, NULL);
    webAddAction(host, SFMT(url, "%s/sig", prefix), sigAction, NULL);
    webAddAction(host, SFMT(url, "%s/buffer", prefix), bufferAction, NULL);
}

#else
PUBLIC void dummyTest(void)
{
}
#endif

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file ../../../src/upload.c ************/

/*
    upload.c -- File upload handler

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/



/*********************************** Forwards *********************************/
#if ME_WEB_UPLOAD

static void freeUpload(WebUpload *up);
static WebUpload *allocUpload(Web *web, cchar *name, cchar *path, cchar *contentType);
static int processUploadData(Web *web, WebUpload *upload);
static WebUpload *processUploadHeaders(Web *web);

/************************************* Code ***********************************/

PUBLIC int webInitUpload(Web *web)
{
    char *boundary;

    if ((boundary = strstr(web->contentType, "boundary=")) != 0) {
        web->boundary = sfmt("--%s", boundary + 9);
        web->boundaryLen = slen(web->boundary);
    }
    if (web->boundaryLen == 0 || *web->boundary == '\0') {
        webError(web, 400, "Bad boundary");
        return R_ERR_BAD_ARGS;
    }
    web->uploads = rAllocHash(0, 0);
    web->numUploads = 0;
    //  Freed in freeWebFields (web.c)
    web->vars = jsonAlloc();
    return 0;
}

PUBLIC void webFreeUpload(Web *web)
{
    RName *np;

    if (web->uploads == 0) return;

    for (ITERATE_NAMES(web->uploads, np)) {
        freeUpload(np->value);
    }
    rFree(web->boundary);
    rFreeHash(web->uploads);
    web->uploads = 0;
}

static void freeUpload(WebUpload *upload)
{
    if (upload->filename) {
        unlink(upload->filename);
        rFree(upload->filename);
    }
    rFree(upload->clientFilename);
    rFree(upload->name);
    rFree(upload->contentType);
    rFree(upload);
}

PUBLIC int webProcessUpload(Web *web)
{
    WebUpload *upload;
    RBuf      *buf;
    char      final[2], suffix[2];
    ssize     nbytes;

    buf = web->rx;
    while (1) {
        if (web->host->maxUploads > 0 && ++web->numUploads > web->host->maxUploads) {
            return webError(web, 413, "Too many files uploaded");
        }
        if ((nbytes = webBufferUntil(web, web->boundary, ME_BUFSIZE, 0)) < 0) {
            return webNetError(web, "Bad upload request boundary");
        }
        rAdjustBufStart(buf, nbytes);

        //  Should be \r\n after the boundary. On the last boundary, it is "--\r\n"
        if (webRead(web, suffix, sizeof(suffix)) < 0) {
            return webNetError(web, "Bad upload request suffix");
        }
        if (sncmp(suffix, "\r\n", 2) != 0) {
            if (sncmp(suffix, "--", 2) == 0) {
                //  End upload, read final \r\n
                if (webRead(web, final, sizeof(final)) < 0) {
                    return webNetError(web, "Cannot read upload request trailer");
                }
                if (sncmp(final, "\r\n", 2) != 0) {
                    return webNetError(web, "Bad upload request trailer");
                }
                break;
            }
            return webNetError(web, "Bad upload request trailer");
        }
        if ((upload = processUploadHeaders(web)) == 0) {
            return R_ERR_CANT_COMPLETE;
        }
        if (processUploadData(web, upload) < 0) {
            return R_ERR_CANT_WRITE;
        }
    }
    web->rxRemaining = 0;
    return 0;
}

static WebUpload *processUploadHeaders(Web *web)
{
    WebUpload *upload;
    char      *content, *field, *filename, *key, *name, *next, *rest, *value, *type;
    ssize     nbytes;

    if ((nbytes = webBufferUntil(web, "\r\n\r\n", ME_BUFSIZE, 0)) < 2) {
        webNetError(web, "Bad upload headers");
        return 0;
    }
    content = web->rx->start;
    content[nbytes - 2] = '\0';
    rAdjustBufStart(web->rx, nbytes);

    /*
        The mime headers may contain Content-Disposition and Content-Type headers
     */
    name = filename = type = 0;
    for (key = content; key && stok(key, "\r\n", &next); ) {
        ssplit(key, ": ", &rest);
        if (scaselessmatch(key, "Content-Disposition")) {
            for (field = rest; field && stok(field, ";", &rest); ) {
                field = strim(field, " ", R_TRIM_BOTH);
                field = ssplit(field, "=", &value);
                value = strim(value, "\"", R_TRIM_BOTH);
                if (scaselessmatch(field, "form-data")) {
                    ;
                } else if (scaselessmatch(field, "name")) {
                    name = value;
                } else if (scaselessmatch(field, "filename")) {
                    if ((filename = webNormalizePath(value)) == 0) {
                        webNetError(web, "Bad upload client filename");
                    }
                }
                field = rest;
            }
        } else if (scaselessmatch(key, "Content-Type")) {
            type = strim(rest, " ", R_TRIM_BOTH);
        }
        key = next;
    }
    if (!(name || filename)) {
        rFree(filename);
        webNetError(web, "Bad multipart mime headers");
        return 0;
    }
    if ((upload = allocUpload(web, name, filename, type)) == 0) {
        rFree(filename);
        return 0;
    }
    rFree(filename);
    return upload;
}

/*
    Path is already normalized
 */
static WebUpload *allocUpload(Web *web, cchar *name, cchar *path, cchar *contentType)
{
    WebUpload *upload;

    assert(name);

    if ((upload = rAllocType(WebUpload)) == 0) {
        return 0;
    }
    web->upload = upload;
    upload->contentType = sclone(contentType);
    upload->name = sclone(name);
    upload->fd = -1;

    if (path) {
        // Enhanced validation against directory traversal
        if (*path == '.' || strstr(path, "..") || strpbrk(path, "\\/:*?<>|~\"'%`^\n\r\t\f")) {
            webError(web, 400, "Bad upload client filename");
            return 0;
        }
        // Check for URL-encoded directory traversal attempts
        if (strstr(path, "%2e") || strstr(path, "%2E") || strstr(path, "%2f") || strstr(path, "%2F") || strstr(path, "%5c") || strstr(path, "%5C")) {
            webError(web, 400, "Bad upload client filename");
            return 0;
        }
        upload->clientFilename = sclone(path);

        /*
            Create the file to hold the uploaded data
         */
        if ((upload->filename = rGetTempFile(web->host->uploadDir, "tmp")) == 0) {
            webError(web, 500, "Cannot create upload temp file. Check upload directory configuration");
            return 0;
        }
        rTrace("web", "File upload of: %s stored as %s", upload->clientFilename, upload->filename);

        if ((upload->fd = open(upload->filename, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0600)) < 0) {
            webError(web, 500, "Cannot open upload temp file");
            return 0;
        }
        rAddName(web->uploads, upload->name, upload, 0);
    }
    return upload;
}

static int processUploadData(Web *web, WebUpload *upload)
{
    RBuf  *buf;
    ssize nbytes, len, written;

    buf = web->rx;

    do {
        if ((nbytes = webBufferUntil(web, web->boundary, ME_BUFSIZE, 1)) < 0) {
            return webNetError(web, "Bad upload request boundary");
        }
        if (upload->filename) {
            /*
                If webBufferUntil returned 0 (short), then a complete boundary was not seen. In this case,
                write the data and continue but preserve a possible partial boundary.
                If a full boundary is found, preserve it (and the \r\n) to be consumed by webProcessUpload.
             */
            len = nbytes ? nbytes : rGetBufLength(web->rx);
            if (nbytes) {
                //  Preserve boundary and \r\n
                len -= web->boundaryLen + 2;
            } else if (memchr(buf->start, web->boundary[0], rGetBufLength(web->rx)) != NULL) {
                //  Preserve a potential partial boundary. i.e. we've seen the start of the boundary only.
                len -= web->boundaryLen - 1 + 2;
            }
            if (len > 0) {
                if ((upload->size + len) > web->host->maxUpload) {
                    return webError(web, 414, "Uploaded file exceeds maximum %lld", web->host->maxUpload);
                }
                if ((written = write(upload->fd, buf->start, len)) < 0) {
                    return webError(web, 500, "Cannot write uploaded file");
                }
                rAdjustBufStart(buf, len);
                upload->size += written;
            }

        } else {
            if (nbytes == 0) {
                return webError(web, 414, "Uploaded form header is too big");
            }
            //  Strip \r\n. Keep boundary in data to be consumed by webProcessUpload.
            nbytes -= web->boundaryLen;
            buf->start[nbytes - 2] = '\0';
            webSetVar(web, upload->name, webDecode(buf->start));
            rAdjustBufStart(buf, nbytes);
        }
    } while (nbytes == 0);

    if (upload->fd >= 0) {
        close(upload->fd);
    }
    return 1;
}
#endif /* ME_WEB_UPLOAD */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file ../../../src/utils.c ************/

/*
    utils.c -

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



/************************************ Locals **********************************/

typedef struct WebStatus {
    int status;                             /**< HTTP error status */
    char *msg;                              /**< HTTP error message */
} WebStatus;

/*
   Standard HTTP status codes
 */
static WebStatus webStatus[] = {
    { 101, "Switching Protocols" },
    { 200, "OK" },
    { 201, "Created" },
    { 204, "No Content" },
    { 205, "Reset Content" },
    { 206, "Partial Content" },
    { 301, "Redirect" },
    { 302, "Redirect" },
    { 304, "Not Modified" },
    { 400, "Bad Request" },
    { 401, "Unauthorized" },
    { 402, "Payment required" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
    { 405, "Unsupported Method" },
    { 406, "Not Acceptable" },
    { 408, "Request Timeout" },
    { 413, "Request too large" },
    { 500, "Internal Server Error" },
    { 501, "Not Implemented" },
    { 503, "Service Unavailable" },
    { 550, "Comms error" },
    { 0, NULL }
};

#define WEB_ENCODE_HTML 0x1                    /* Bit setting in charMatch[] */
#define WEB_ENCODE_URI  0x4                    /* Encode URI characters */

/*
    Character escape/descape matching codes. Generated by mprEncodeGenerate in appweb.
 */
static uchar charMatch[256] = {
    0x00, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x7e, 0x3c, 0x3c, 0x7c, 0x3c, 0x3c,
    0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x7c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
    0x3c, 0x00, 0x7f, 0x28, 0x2a, 0x3c, 0x2b, 0x43, 0x02, 0x02, 0x02, 0x28, 0x28, 0x00, 0x00, 0x28,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x2a, 0x3f, 0x28, 0x3f, 0x2a,
    0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3a, 0x7e, 0x3a, 0x3e, 0x00,
    0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x3e, 0x3e, 0x02, 0x3c,
    0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
    0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
    0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
    0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
    0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
    0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
    0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
    0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c
};

/************************************* Code ***********************************/

PUBLIC cchar *webGetStatusMsg(int status)
{
    WebStatus *sp;

    if (status < 0 || status > 599) {
        return "Unknown";
    }
    for (sp = webStatus; sp->msg; sp++) {
        if (sp->status == status) {
            return sp->msg;
        }
    }
    return "Unknown";
}

PUBLIC char *webDate(char *buf, time_t when)
{
    struct tm tm;

    buf[0] = '\0';
    if (gmtime_r(&when, &tm) == 0) {
        return 0;
    }
    if (asctime_r(&tm, buf) == 0) {
        return 0;
    }
    buf[strlen(buf) - 1] = '\0';
    return buf;
}

PUBLIC cchar *webGetDocs(WebHost *host)
{
    return host->docs;
}

/*
    URL encoding decode
 */
PUBLIC char *webDecode(char *str)
{
    char  *ip,  *op;
    ssize len;
    int   num, i, c;

    if (str == 0) {
        return 0;
    }
    len = slen(str);
    for (ip = op = str; len > 0 && *ip; ip++, op++) {
        if (*ip == '+') {
            *op = ' ';
        } else if (*ip == '%' && isxdigit((uchar) ip[1]) && isxdigit((uchar) ip[2]) &&
                   !(ip[1] == '0' && ip[2] == '0')) {
            // Convert %nn to a single character
            ip++;
            for (i = 0, num = 0; i < 2; i++, ip++) {
                c = tolower((uchar) * ip);
                if (c >= 'a' && c <= 'f') {
                    num = (num * 16) + 10 + c - 'a';
                } else {
                    num = (num * 16) + c - '0';
                }
            }
            *op = (char) num;
            ip--;

        } else {
            *op = *ip;
        }
        len--;
    }
    *op = '\0';
    return str;
}


/*
    Note: the path does not contain a leading "/". Similarly, the query and hash do not contain the ? or #
 */
PUBLIC char *webParseUrl(cchar *uri,
                         cchar **scheme,
                         cchar **host,
                         int *port,
                         cchar **path,
                         cchar **query,
                         cchar **hash)
{
    char *buf, *end, *next, *tok;

    if (scheme) *scheme = 0;
    if (host) *host = 0;
    if (port) *port = 0;
    if (path) *path = 0;
    if (query) *query = 0;
    if (hash) *hash = 0;

    tok = buf = sclone(uri);

    //  hash comes after the query
    if ((next = schr(tok, '#')) != 0) {
        *next++ = '\0';
        if (hash) {
            *hash = next;
        }
    }
    if ((next = schr(tok, '?')) != 0) {
        *next++ = '\0';
        if (query) {
            *query = next;
        }
    }
    if (!schr(tok, '/') && (smatch(tok, "http") || smatch(tok, "https") || smatch(tok, "ws") || smatch(tok, "wss"))) {
        //  No hostname or path
        if (scheme) {
            *scheme = tok;
        }
    } else {
        if ((next = scontains(tok, "://")) != 0) {
            if (scheme) {
                *scheme = tok;
            }
            *next = 0;
            if (smatch(tok, "https") || smatch(tok, "wss")) {
                if (port) {
                    *port = 443;
                }
            }
            tok = &next[3];
        }
        if (*tok == '[' && ((next = strchr(tok, ']')) != 0)) {
            /* IPv6  [::]:port/url */
            if (host) {
                *host = &tok[1];
            }
            *next++ = 0;
            tok = next;

        } else if (*tok && *tok != '/') {
            // hostname:port/path
            if (host) {
                *host = tok;
            }
            if ((tok = spbrk(tok, ":/")) != 0) {
                if (*tok == ':') {
                    *tok++ = 0;
                    if (port) {
                        *port = (int) strtol(tok, &end, 10);
                        if (end == tok || (*end != '\0' && *end != '/')) {
                            //  Invalid characters in port
                            rFree(buf);
                            return 0;
                        }
                        if (*port <= 0 || *port > 65535) {
                            rFree(buf);
                            return 0;
                        }
                    }
                    if ((tok = schr(tok, '/')) == 0) {
                        tok = "";
                    }
                } else {
                    if (tok[0] == '/' && tok[1] == '\0' && path) {
                        //  Bare path "/"
                        *path = "";
                    }
                    *tok++ = 0;
                }
            }
        }
        if (tok && *tok && path) {
            if (*tok == '/') {
                *path = ++tok;
            } else {
                *path = tok;
            }
        }
    }
    if (host && *host && !*host[0]) {
        //  Empty hostnames are meaningless
        *host = 0;
    }
    return buf;
}

/*
    Normalize a path to remove "./",  "../" and redundant separators.
    This does not map separators nor change case. Returns an allocated path, caller must free.

    SECURITY Acceptable:: This routine does not check for path traversal because
    all callers validate the path before calling this routine.
 */
PUBLIC char *webNormalizePath(cchar *pathArg)
{
    char  *dupPath, *path, *sp, *mark, **segments, **stack;
    ssize len, nseg, i, j;
    bool  isAbs, hasTrail;

    if (pathArg == 0 || *pathArg == '\0') {
        return 0;
    }
    len = slen(pathArg);
    dupPath = sclone(pathArg);
    isAbs = (pathArg[0] == '/');
    hasTrail = (len > 1 && pathArg[len - 1] == '/');

    //  Split path into segments
    if ((segments = rAlloc(sizeof(char*) * (len + 2))) == 0) {
        rFree(dupPath);
        return 0;
    }
    for (nseg = 0, mark = sp = dupPath; *sp; sp++) {
        if (*sp == '/') {
            *sp = '\0';
            segments[nseg++] = mark;
            while (sp[1] == '/') {
                sp++;
            }
            mark = sp + 1;
        }
    }
    // Add the last segment
    segments[nseg++] = mark;

    if ((stack = rAlloc(sizeof(char*) * (nseg + 1))) == 0) {
        rFree(dupPath);
        rFree(segments);
        return 0;
    }
    for (j = 0, i = 0; i < nseg; i++) {
        sp = segments[i];
        if (*sp == '\0' || smatch(sp, ".")) {
            continue;
        }
        if (smatch(sp, "..")) {
            if (j > 0) {
                j--;
            } else {
                //  Attempt to traverse up from a relative path's root. This is a security risk.
                rFree(dupPath);
                rFree(segments);
                rFree(stack);
                return NULL;
            }
        } else {
            stack[j++] = sp;
        }
    }
    nseg = j;

    //  Rebuild the path
    RBuf *buf = rAllocBuf(len + 2);
    if (isAbs) {
        rPutCharToBuf(buf, '/');
    }
    for (i = 0; i < nseg; i++) {
        rPutStringToBuf(buf, stack[i]);
        if (i < nseg - 1) {
            rPutCharToBuf(buf, '/');
        }
    }
    if (hasTrail && rGetBufLength(buf) > 0 && rLookAtLastCharInBuf(buf) != '/') {
        rPutCharToBuf(buf, '/');
    }
    if (rGetBufLength(buf) == 0) {
        if (isAbs) {
            rPutCharToBuf(buf, '/');
        } else {
            rPutCharToBuf(buf, '.');
        }
    }
    path = rBufToStringAndFree(buf);

    rFree(dupPath);
    rFree(segments);
    rFree(stack);
    return path;
}

/*
    Escape HTML to escape defined characters (prevent cross-site scripting). Returns an allocated string.
 */
PUBLIC char *webEscapeHtml(cchar *html)
{
    RBuf *buf;

    if (!html) {
        return sclone("");
    }
    buf = rAllocBuf(slen(html) + 1);
    while (*html != '\0') {
        switch (*html) {
        case '&':
            rPutStringToBuf(buf, "&amp;");
            break;
        case '<':
            rPutStringToBuf(buf, "&lt;");
            break;
        case '>':
            rPutStringToBuf(buf, "&gt;");
            break;
        case '#':
            rPutStringToBuf(buf, "&#35;");
            break;
        case '(':
            rPutStringToBuf(buf, "&#40;");
            break;
        case ')':
            rPutStringToBuf(buf, "&#41;");
            break;
        case '"':
            rPutStringToBuf(buf, "&quot;");
            break;
        case '\'':
            rPutStringToBuf(buf, "&#39;");
            break;
        default:
            rPutCharToBuf(buf, *html);
            break;
        }
        html++;
    }
    return rBufToStringAndFree(buf);
}

/*
    Uri encode by encoding special characters with hex equivalents. Return an allocated string.
 */
PUBLIC char *webEncode(cchar *uri)
{
    static cchar hexTable[] = "0123456789ABCDEF";
    uchar        c;
    cchar        *ip;
    char         *result, *op;
    int          len;

    if (!uri) {
        return NULL;
    }
    for (len = 1, ip = uri; *ip; ip++, len++) {
        if (charMatch[(uchar) * ip] & WEB_ENCODE_URI) {
            len += 2;
        }
    }
    if ((result = rAlloc(len)) == 0) {
        return 0;
    }
    op = result;

    while ((c = (uchar) (*uri++)) != 0) {
        if (charMatch[c] & WEB_ENCODE_URI) {
            *op++ = '%';
            *op++ = hexTable[c >> 4];
            *op++ = hexTable[c & 0xf];
        } else {
            *op++ = c;
        }
    }
    assert(op < &result[len]);
    *op = '\0';
    return result;
}

PUBLIC Json *webParseJson(Web *web)
{
    Json *json;
    char *errorMsg;

    if ((json = jsonParseString(rBufToString(web->body), &errorMsg, 0)) == 0) {
        rError("web", "Cannot parse json: %s", errorMsg);
        rFree(errorMsg);
        return 0;
    }
    return json;
}

PUBLIC void webParseEncoded(Web *web, Json *vars, cchar *str)
{
    char *data, *keyword, *value, *tok;

    data = sclone(str);
    keyword = stok(data, "&", &tok);
    while (keyword != NULL) {
        if ((value = strchr(keyword, '=')) != NULL) {
            *value++ = '\0';
            webDecode(keyword);
            webDecode(value);
        } else {
            value = "";
        }
        jsonSet(vars, 0, keyword, value, 0);
        keyword = stok(tok, "&", &tok);
    }
    rFree(data);
}

PUBLIC void webParseQuery(Web *web)
{
    if (web->query) {
        webParseEncoded(web, web->qvars, web->query);
    }
}

PUBLIC void webParseForm(Web *web)
{
    webParseEncoded(web, web->vars, rBufToString(web->body));
}

/*
    Get a request variable from the form/body request.
 */
PUBLIC cchar *webGetVar(Web *web, cchar *name, cchar *defaultValue)
{
    return jsonGet(web->vars, 0, name, defaultValue);
}

/*
    Set a request variable to augment the form/body request.
 */
PUBLIC void webSetVar(Web *web, cchar *name, cchar *value)
{
    assert(web->vars);
    jsonSet(web->vars, 0, name, value, 0);
}

/*
    Remove a request variable from the form/body request.
 */
PUBLIC void webRemoveVar(Web *web, cchar *name)
{
    jsonRemove(web->vars, 0, name);
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file ../../../src/validate.c ************/

/*
    validate.c - Validate request and response signatures

    The description, notes, private, title and name fields are generally for documentation
    tools and are ignored by the validation routines.

    Documentation:
        Model Name (controller name)
        Model Description (meta.description)
        Model Notes (meta.notes)
        Model See Also (meta.see)
        Method
            Method Name
            Method Title
            Method Description
            Method Method
            Role
            Request Section
            Query Section
            Response Section
                ${name} Record
                notes - go into paged response


    Format of the signatures.json5 file:
    {
        CONTROLLER_NAME: {
            _meta: {
                description:         - Doc markdown description
                notes:               - Notes for doc
                see:                 - See also
                private:             - Hide from doc
                role:                - Access control
                title:               - Short title
            },
            METHOD: {
                description:        - Doc markdown description
                example             - Example value
                private:            - Hide from doc
                role:               - Access control
                request:            - BLOCK, null, 'string', 'number', 'boolean', 'array', 'object'
                response:           - BLOCK, null, 'string', 'number', 'boolean', 'array', 'object'
                request.query:      - BLOCK, null, 'string', 'number', 'boolean', 'array', 'object'
                title:              - Short title

                BLOCK: {
                    type: 'object' | 'array' | 'string' | 'number' | 'boolean' | 'null'
                    fields: {       - If object
                        NAME: {
                            description - Field description for doc
                            drop        - Remove from data (for response)
                            fields      - Nested object
                            of          - If type == 'array' then nested block
                            required    - Must be present (for request)
                            role        - Discard if role not posessed
                            type        - Type (object, array, string, number, boolean)
                            validate    - Regexp
                        },
                    }
                    array: {            - If array
                        of: {
                            type:       - Data type
                            fields: {}, - Item object
                        }
                    },
                }
                request.query:           - BLOCK, null, 'string', 'number', 'boolean', 'array', 'object'
        }
    }

    Notes:
    - Can always omit the response and query blocks and the data is then unvalidated
    - If not strict can omit request blocks and the data is then unvalidated, but will warn via the log.
    - Can use block: null and all fields are unvalidated
    - Can use {type: 'object'} without fields and all fields are unvalidated
    - Fields set to {} means no fields are defined

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



/************************************ Locals **********************************/

#define WEB_MAX_SIG_DEPTH 8  /* Maximum depth of signature validation */

/************************************ Forwards *********************************/

static cchar *getType(Web *web, int sid);
static int parseUrl(Web *web);
static bool valError(Web *web, Json *json, cchar *fmt, ...);
static bool validateArray(Web *web, RBuf *buf, Json *json, int jid, int sid, int depth, cchar *tag);
static bool validateObject(Web *web, RBuf *buf, Json *json, int jid, int sid, int depth, cchar *tag);
static bool validatePrimitive(Web *web, cchar *data, int sid, cchar *tag);
static bool validateProperty(Web *web, RBuf *buf, Json *json, int jid, int sid, cchar *tag);

/************************************* Code ***********************************/
/*
    Validate the request using a URL request path and the host->signatures.
    The path is used as a JSON property path into the signatures.json5 file.
    Generally, this is the controller.method format, but custom formats are supported.
    This routine will generate an error response if the signature is not found or if
    the signature is invalid. Depending on the host->strictSignatures flag, it will either
    return 0 or R_ERR_BAD_REQUEST. If invalid, the connection is closed after the error
    response is written.
 */
PUBLIC bool webValidateRequest(Web *web, cchar *path)
{
    WebHost *host;
    cchar   *type;
    int     sid;

    host = web->host;
    if (!host->signatures) {
        return 0;
    }
    if (web->signature < 0) {
        if (host->strictSignatures) {
            return valError(web, NULL, "Missing request signature for request");
        }
        rDebug("web", "Cannot find signature for %s, continuing.", web->path);
        return 1;
    }
    //  Optional query signature
    if (web->qvars && (sid = jsonGetId(host->signatures, web->signature, "request.query")) >= 0) {
        return webValidateSignature(web, NULL, web->qvars, 0, sid, 0, "query");
    }
    if ((sid = jsonGetId(host->signatures, web->signature, "request")) < 0) {
        if (host->strictSignatures) {
            return valError(web, NULL, "Missing request API signature");
        }
        rDebug("web", "Cannot find request signature for %s, continuing.", web->path);
        return 1;
    }
    type = getType(web, sid);
    if (smatch(type, "object") || smatch(type, "array")) {
        return webValidateSignature(web, NULL, web->vars, 0, sid, 0, "request");
    }
    return validatePrimitive(web, rBufToString(web->body), sid, "request");
}

/*
    Check a JSON payload against the API signature. This evaluates the json properties starting at the "jid" node.
    If buf is provided, it will be used to store the validated JSON. If fields are "dropped" they will not be
    added to the buf. The routine will recurse as required over arrays and objects.
    Returns true if the payload is valid. If invalid, we return 0 and a response is written.

    Signature BLOCKS are of the form: {
        type: 'null', 'string', 'number', 'boolean', 'object', 'array'
        fields: {},
        of: BLOCK
    }

    @param web Web object
    @param buf Optional buffer to store the validated JSON as a string ready for writing to the client.
    @param cjson JSON object (May be null)
    @param jid Base JSON node ID from which to convert. Set to zero for the top level. If NULL, the top level is used.
    @param sid Signature ID to validate against
    @param depth Depth of the JSON object
    @param tag Tag name for the request body. Set to "request", "response" or "query".
    @return True if the request is valid. Otherwise, return false and generate an error response to the client.
 */
PUBLIC bool webValidateSignature(Web *web, RBuf *buf, const Json *cjson, int jid, int sid, int depth, cchar *tag)
{
    Json     *json;
    JsonNode *item;
    cchar    *type, *value;

    assert(web);
    assert(tag);

    if (!web || !tag || jid < 0) {
        rError("web", "Invalid parameters to validateSignature");
        return 0;
    }
    if (!web->host->signatures || sid < 0) {
        return 1;
    }
    if (depth > WEB_MAX_SIG_DEPTH) {
        webError(web, 400, "Signature validation failed");
        return 0;
    }
    // May be null
    json = (Json*) cjson;
    type = getType(web, sid);

    if (smatch(type, "array")) {
        if (!validateArray(web, buf, json, jid, sid, depth, tag)) {
            return 0;
        }

    } else if (smatch(type, "object")) {
        if (!validateObject(web, buf, json, jid, sid, depth, tag)) {
            return 0;
        }
    } else {
        //  Primitive value property
        if (json) {
            item = jsonGetNode(json, jid, 0);
            value = item ? item->value : NULL;
        } else {
            value = NULL;
        }
        if (!validatePrimitive(web, value, sid, tag)) {
            return 0;
        }
        if (buf && value) {
            jsonPutValueToBuf(buf, value, JSON_JSON);
        }
    }
    return 1;
}

/*
    Iterate over the array items
 */
static bool validateArray(Web *web, RBuf *buf, Json *json, int jid, int sid, int depth, cchar *tag)
{
    Json     *signatures;
    JsonNode *array, *item;
    cchar    *oftype;
    int      oid;

    signatures = web->host->signatures;
    if (!json) {
        // Allow an empty array
        return 1;
    }
    array = jsonGetNode(json, jid, 0);

    if (!array || array->type != JSON_ARRAY) {
        return valError(web, NULL, "Bad %s, expected an array", tag);
    }
    if (buf) {
        rPutCharToBuf(buf, '[');
    }
    for (ITERATE_JSON_ID(json, jid, item, iid)) {
        oid = jsonGetId(signatures, sid, "of");
        if (oid >= 0) {
            oftype = jsonGet(signatures, oid, "type", "object");
            if (smatch(oftype, "object") || smatch(oftype, "array")) {
                if (!webValidateSignature(web, buf, json, iid, oid, depth + 1, tag)) {
                    return 0;
                }
            } else {
                if (!validatePrimitive(web, item->value, oid, tag)) {
                    return 0;
                }
                if (buf) {
                    jsonPutValueToBuf(buf, item->value, JSON_JSON);
                }
            }
        } else {
            // Allow untyped array without a signature "of" block
            if (buf) {
                jsonPutToBuf(buf, json, iid, JSON_JSON);
            }
        }
        if (buf) {
            rPutCharToBuf(buf, ',');
        }
    }
    if (buf) {
        if (rGetBufLength(buf) > 1) {
            rAdjustBufEnd(buf, -1);
        }
        rPutCharToBuf(buf, ']');
    }
    return 1;
}

/*
    Validate a object properties and write to the optional buffer.
    The json object may be NULL to indicate no body.
 */
static bool validateObject(Web *web, RBuf *buf, Json *json, int jid, int sid, int depth, cchar *tag)
{
    Json     *signatures;
    JsonNode *drop, *field, *fields, *parent, *var;
    cchar    *def, *dropRole, *ftype, *methodRole, *required, *role, *value;
    char     dropBuf[128];
    bool     hasRequired, hasWild, strict;
    int      fid, id, fieldsId;

    signatures = web->host->signatures;
    strict = web->host->strictSignatures;

    fields = jsonGetNode(signatures, sid, "fields");
    if (!fields) {
        //  Generic object with no fields defined
        if (buf && json) {
            jsonPutToBuf(buf, json, jid, JSON_JSON);
        }
        return 1;
    }
    hasWild = jsonGetBool(signatures, sid, "hasWild", 0);            // Allow any properties
    hasRequired = jsonGetBool(signatures, sid, "hasRequired", 0);    // Signature has required fields
    methodRole = jsonGet(signatures, sid, "role", web->route->role); // Required role for this signature

    if (buf) {
        rPutCharToBuf(buf, '{');
    }
    if (hasRequired) {
        /*
            Ensure all required fields are present.
         */
        for (ITERATE_JSON(signatures, fields, field, fid)) {
            required = jsonGet(signatures, fid, "required", 0);
            if (required) {
                if (!json) {
                    return valError(web, NULL, "Missing required %s field '%s'", tag, field->name);
                }
                value = jsonGet(json, jid, field->name, 0);
                if (!value) {
                    def = jsonGet(signatures, fid, "default", 0);
                    if (required && !def) {
                        return valError(web, json, "Missing required %s field '%s'", tag, field->name);
                    }
                    if (def) {
                        if (buf) {
                            // Add default value
                            jsonPutValueToBuf(buf, field->name, JSON_JSON);
                            rPutCharToBuf(buf, ':');
                            jsonPutValueToBuf(buf, def, JSON_JSON);
                            rPutCharToBuf(buf, ',');
                        } else {
                            // Add default value to the request / query json object
                            rassert(!smatch(tag, "response"));
                            jsonSet(json, jid, field->name, def, JSON_STRING);
                        }
                    }
                }
            }
        }
    }
    fieldsId = jsonGetNodeId(signatures, fields);

    if (json) {
        parent = jsonGetNode(json, jid, 0);
        for (ITERATE_JSON(json, parent, var, vid)) {
            if (var->name[0] == '_' || smatch(var->name, "pk") || smatch(var->name, "sk")) {
                // Always hidden
                continue;
            }
            fid = jsonGetId(signatures, fieldsId, var->name);
            if (fid < 0 && !hasWild) {
                if (strict) {
                    return valError(web, json, "Invalid %s field '%s' in %s", tag, var->name, web->url);
                }
                rDebug("web", "Invalid %s field '%s' in %s", tag, var->name, web->url);
                continue;
            }
            role = jsonGet(signatures, fid, "role", methodRole);
            if (role && !webCan(web, role)) {
                // Silently drop if role does not permit access
                continue;
            }
            drop = jsonGetNode(signatures, fid, "drop");
            if (drop != 0) {
                if (drop->type == JSON_PRIMITIVE && smatch(drop->value, "true")) {
                    continue;
                } else if (drop->type == JSON_STRING) {
                    if (!webCan(web, drop->value)) {
                        continue;
                    }
                } else if (drop->type == JSON_OBJECT) {
                    dropRole = jsonGet(signatures, fid, SFMT(dropBuf, "drop.%s", tag), 0);
                    if (dropRole && !webCan(web, dropRole)) {
                        continue;
                    }
                }
            }
            if (buf) {
                jsonPutValueToBuf(buf, var->name, JSON_JSON);
                rPutCharToBuf(buf, ':');
            }
            ftype = jsonGet(signatures, fid, "type", 0);
            if (smatch(ftype, "object")) {
                id = jsonGetId(json, jid, var->name);
                if (!webValidateSignature(web, buf, json, id, fid, depth + 1, tag)) {
                    return 0;
                }
            } else if (smatch(ftype, "array")) {
                id = jsonGetId(json, jid, var->name);
                if (!webValidateSignature(web, buf, json, id, fid, depth + 1, tag)) {
                    return 0;
                }
            } else if (!validateProperty(web, buf, json, vid, fid, tag)) {
                return 0;
            }
            if (buf) {
                rPutCharToBuf(buf, ',');
            }
        }
    }
    // Remove trailing comma
    if (buf) {
        if (rGetBufLength(buf) > 1) {
            rAdjustBufEnd(buf, -1);
        }
        rPutCharToBuf(buf, '}');
    }
    return 1;
}

/*
    Validate a primitive value property and write to the optional buffer.
 */
static bool validateProperty(Web *web, RBuf *buf, Json *json, int jid, int sid, cchar *tag)
{
    JsonNode *item;

    item = jsonGetNode(json, jid, 0);
    if (!validatePrimitive(web, item ? item->value : NULL, sid, tag)) {
        return 0;
    }
    if (buf) {
        jsonPutValueToBuf(buf, item ? item->value : NULL, JSON_JSON);
    }
    return 1;
}

/*
    Validate data for primitive types against the API signature.
 */
static bool validatePrimitive(Web *web, cchar *data, int sid, cchar *tag)
{
    JsonNode *signature;
    cchar    *type;

    assert(web);
    assert(tag);

    if (!web->host->signatures || sid < 0) {
        return 0;
    }
    type = getType(web, sid);
    if ((signature = jsonGetNode(web->host->signatures, sid, 0)) == 0) {
        return 0;
    }
    if (smatch(type, "null")) {
        if (data && *data) {
            return valError(web, NULL, "Bad %s, data should be empty", tag);
        }
    } else {
        if (!data) {
            return valError(web, NULL, "Missing %s data, expected %s", tag, type);
        }
        if (smatch(type, "string")) {
            // Most common case first

        } else if (smatch(type, "number")) {
            if (!sfnumber(data)) {
                return valError(web, NULL, "Bad %s, \"%s\" should be a number", tag, signature->name);
            }

        } else if (smatch(type, "boolean")) {
            if (!scaselessmatch(data, "true") && !scaselessmatch(data, "false")) {
                return valError(web, NULL, "Bad %s, \"%s\" should be a boolean", tag, signature->name);
            }

        } else if (smatch(type, "date")) {
            if (rParseIsoDate(data) < 0) {
                return valError(web, NULL, "Bad %s, \"%s\" should be a date", tag, signature->name);
            }

        } else {
            return valError(web, NULL, "Bad %s data, expected a %s for \"%s\"", tag, type, signature->name);
        }
        /* object | array */
    }
    return 1;
}

/*
    Validate a data primitive against the API signature and write to the optional buffer. Return true if valid.
 */
PUBLIC bool webValidateData(Web *web, RBuf *buf, cchar *data, cchar *sigKey, cchar *tag)
{
    Json  *json;
    cchar *type;
    int   rc, sid;

    if (!web->host->signatures) {
        return 1;
    }
    if (sigKey) {
        sid = jsonGetId(web->host->signatures, 0, sigKey);
        if (sid < 0) {
            return valError(web, NULL, "Missing signature for %s", web->url);
        }
    } else {
        sid = jsonGetId(web->host->signatures, web->signature, "response");
        if (sid < 0) {
            // Allow a signature to omit the response field (even with strict mode)
            return 1;
        }
        type = getType(web, sid);
        if (smatch(type, "object") || smatch(type, "array")) {
            json = jsonParse(data, 0);
            rc = webValidateSignature(web, buf, json, 0, sid, 0, tag);
            jsonFree(json);
            return rc;
        }
    }
    if (!validatePrimitive(web, data, sid, tag)) {
        return 0;
    }
    if (buf) {
        jsonPutValueToBuf(buf, data, JSON_JSON);
    }
    return 1;
}

/*
    Validate json against the API signature and write to the optional buffer. Return true if valid.
 */
PUBLIC bool webValidateJson(Web *web, RBuf *buf, const Json *cjson, int jid, cchar *sigKey, cchar *tag)
{
    int sid;

    if (!web->host->signatures) {
        return 1;
    }
    if (sigKey) {
        sid = jsonGetId(web->host->signatures, 0, sigKey);
        if (sid < 0) {
            return 0;
        }
    } else {
        sid = jsonGetId(web->host->signatures, web->signature, "response");
        if (sid < 0) {
            // Allow a signature to omit the response field (even with strict mode)
            if (buf) {
                jsonPutToBuf(buf, cjson, jid, JSON_JSON);
            }
            return 1;
        }
    }
    return webValidateSignature(web, buf, cjson, jid, sid, 0, tag);
}

/*
    Validate a data buffer against an API signature. The standard response signature is used if no key is provided.
    We allow a signature to omit the response field (even with strict mode)
 */
PUBLIC ssize webWriteValidatedData(Web *web, cchar *data, cchar *sigKey)
{
    assert(web);

    webBuffer(web, 0);
    if (!webValidateData(web, web->buffer, data, sigKey, "response")) {
        return R_ERR_BAD_ARGS;
    }
    return rGetBufLength(web->buffer);
}

/*
    Validate a json object against an API signature. The standard response signature is used if no key is provided.
 */
PUBLIC ssize webWriteValidatedJson(Web *web, const Json *json, cchar *sigKey)
{
    assert(web);

    webBuffer(web, 0);
    if (!webValidateJson(web, web->buffer, json, 0, sigKey, "response")) {
        return R_ERR_BAD_ARGS;
    }
    return rGetBufLength(web->buffer);
}

/*
    Check a URL path for valid characters
 */
PUBLIC bool webValidatePath(cchar *path)
{
    ssize pos;

    if (!path || *path == '\0') {
        return 0;
    }
    pos = strspn(path, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~:/?#[]@!$&'()*+,;=%");
    return pos < slen(path) ? 0 : 1;
}

/*
    Validate the request URL
 */
PUBLIC int webValidateUrl(Web *web)
{
    if (web->url == 0 || *web->url == 0) {
        return webNetError(web, "Empty URL");
    }
    if (!webValidatePath(web->url)) {
        webNetError(web, "Bad characters in URL");
        return R_ERR_BAD_ARGS;
    }
    if (parseUrl(web) < 0) {
        //  Already set error
        return R_ERR_BAD_ARGS;
    }
    return 0;
}

/*
    Decode and parse the request URL
 */
static int parseUrl(Web *web)
{
    char *delim, *path, *tok;

    if (web->url == 0 || *web->url == '\0') {
        return webNetError(web, "Empty URL");
    }
    //  Hash comes after the query
    path = web->url;
    stok(path, "#", &web->hash);
    stok(path, "?", &web->query);

    if ((tok = strrchr(path, '.')) != 0) {
        if (tok[1]) {
            if ((delim = strrchr(path, '/')) != 0) {
                if (delim < tok) {
                    web->ext = tok;
                }
            } else {
                web->ext = webDecode(tok);
            }
        }
    }
    //  Query is decoded when parsed in webParseQuery and webParseEncoded
    webDecode(path);
    webDecode(web->hash);

    /*
        Normalize and sanitize the path. This routine will process ".." and "." segments.
        This is safe because callers (webFileHandler) uses simple string concatenation to
        join the result with the document root.
     */
    if ((web->path = webNormalizePath(path)) == 0) {
        return webNetError(web, "Illegal URL");
    }
    return 0;
}

static cchar *getType(Web *web, int sid)
{
    JsonNode *signature;
    cchar    *type;

    if ((signature = jsonGetNode(web->host->signatures, sid, 0)) == 0) {
        return "object";
    }
    if (signature->type == JSON_PRIMITIVE && smatch(signature->value, "null")) {
        type = "null";
    } else if (signature->type == JSON_STRING) {
        type = signature->value;
    } else {
        sid = jsonGetNodeId(web->host->signatures, signature);
        type = jsonGet(web->host->signatures, sid, "type", 0);
    }
    if (!type) {
        type = "object";
    }
    return type;
}

/*
    This will write an error response to the client and close the connection.
 */
static bool valError(Web *web, Json *json, cchar *fmt, ...)
{
    va_list args;
    char    *msg;

    va_start(args, fmt);
    msg = sfmtv(fmt, args);
    webWriteResponse(web, 0, "%s\n", msg);
    if (json) {
        rDebug("web", "Validation payload\n%s", jsonString(json, JSON_HUMAN));
    }
    rFree(msg);
    va_end(args);
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */

#else
void dummyWeb(){}
#endif /* ME_COM_WEB */
