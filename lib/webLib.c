/*
 * Embedthis Web Library Source
 */

#include "web.h"

#if ME_COM_WEB



/********* Start of file src/auth.c ************/

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
            Retrieve authentication state from the session storage. Faster than re-authenticating.
         */
        if ((web->username = webGetSessionVar(web, WEB_SESSION_USERNAME, 0)) != 0) {
            web->role = (char*) webGetSessionVar(web, WEB_SESSION_ROLE, 0);
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

    if (!requiredRole || *requiredRole == '\0') {
        return 1;
    }
    host = web->host;

    if (!web->authenticated && !webAuthenticate(web)) {
        webError(web, 401, "Access Denied. User not logged in.");
        return 0;
    }
    roleId = jsonGetId(host->config, host->roles, requiredRole);
    if (roleId < 0) {
        rError("web", "Unknown role %s", requiredRole);
    }
    if (roleId < 0 || roleId > web->roleId) {
        webError(web, 401, "Authorization Denied.");
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

    rTrace("auth", "Login successful for %s, role %s", username, role);
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


/********* Start of file src/file.c ************/

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
static int sumPath(cchar *path);

/************************************* Code ***********************************/

PUBLIC int webFileHandler(Web *web)
{
    FileInfo info;
    char     *path;
    int      rc;

    path = sjoin(webGetDocs(web->host), web->path, NULL);

    web->exists = stat(path, &info) == 0;
    web->ext = strrchr(path, '.');

    if (smatch(web->method, "GET") || smatch(web->method, "HEAD") || smatch(web->method, "POST")) {
        rc = getFile(web, path, &info);

    } else if (smatch(web->method, "PUT")) {
        rc = putFile(web, path);

    } else if (smatch(web->method, "DELETE")) {
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
    char  date[32], *lpath;
    int64 ino;
    int   fd;

    if (!web->exists) {
        webHook(web, WEB_HOOK_NOT_FOUND);
        if (!web->complete) {
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
    ino = info->st_ino ? info->st_ino : sumPath(path);
    webAddHeader(web, "ETag", "%Ld", ino + info->st_size + (int64) info->st_mtime);

    if (smatch(web->method, "HEAD")) {
        webWrite(web, 0, 0);
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

    if ((fd = open(path, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, 0644)) < 0) {
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

static int sumPath(cchar *path)
{
    cchar *cp;
    int   sum = 0;

    for (cp = path; *cp; cp++) {
        sum += *cp;
    }
    return sum;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file src/host.c ************/

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
    cchar   *show;
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
        if ((config = jsonParseFile(ME_WEB_CONFIG, &errorMsg, JSON_LOCK)) == 0) {
            rError("config", "%s", errorMsg);
            rFree(errorMsg);
            return 0;
        }
        host->freeConfig = 1;
    }
    host->config = config;

    host->index = jsonGet(host->config, 0, "web.index", "index.html");
    host->parseTimeout = getTimeout(host, "web.timeouts.parse", "5secs");
    host->inactivityTimeout = getTimeout(host, "web.timeouts.inactivity", "5mins");
    host->requestTimeout = getTimeout(host, "web.timeouts.request", "5mins");
    host->sessionTimeout = getTimeout(host, "web.timeouts.session", "30mins");

#if ME_WEB_LIMITS
    host->maxHeader = svalue(jsonGet(host->config, 0, "web.limits.header", "10K"));
    host->maxConnections = svalue(jsonGet(host->config, 0, "web.limits.connections", "100"));
    host->maxBody = svalue(jsonGet(host->config, 0, "web.limits.body", "100K"));
    host->maxSessions = svalue(jsonGet(host->config, 0, "web.limits.sessions", "20"));
    host->maxUpload = svalue(jsonGet(host->config, 0, "web.limits.upload", "20MB"));
#endif

    host->docs = rGetFilePath(jsonGet(host->config, 0, "web.documents", "@site"));
    host->name = jsonGet(host->config, 0, "web.name", 0);
    host->uploadDir = jsonGet(host->config, 0, "web.upload.dir", uploadDir());
    host->sameSite = jsonGet(host->config, 0, "web.sessions.sameSite", "Lax");
    host->httpOnly = jsonGetBool(host->config, 0, "web.sessions.httpOnly", 0);
    host->roles = jsonGetId(host->config, 0, "web.auth.roles");
    host->headers = jsonGetId(host->config, 0, "web.headers");

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
        rFree(route->match);
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
    rFree(host->docs);
    rFree(host->ip);
    rFree(host);
}

PUBLIC int webStartHost(WebHost *host)
{
    Json      *json;
    WebListen *listen;
    JsonNode  *endpoints, *np;
    cchar     *endpoint;
    int       id;

    if (!host || !host->listeners) return 0;

    json = host->config;
    endpoints = jsonGetNode(json, 0, "web.listen");

    if (endpoints) {
        for (ITERATE_JSON(json, endpoints, np, id)) {
            endpoint = jsonGet(json, id, 0, 0);
            if ((listen = allocListen(host, endpoint)) == 0) {
                return R_ERR_CANT_OPEN;
            }
            rAddItem(host->listeners, listen);
        }
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
    char      *hostname, *sport, *scheme, *tok;
    int       port;

    if ((listen = rAllocType(WebListen)) == 0) {
        rError("web", "Cannot allocate memory for WebListen");
        return 0;
    }
    listen->host = host;
    listen->endpoint = sclone(endpoint);
    rInfo("web", "Listening %s", endpoint);

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
        hostname = "localhost";
        sport = "80";
    }
    port = atoi(sport);

    if (port == 0) {
        rError("web", "Bad or missing port %d in Listen directive", port);
        rFree(tok);
        return 0;
    }
    if (*hostname == 0) {
        hostname = NULL;
    }
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

static int getTimeout(WebHost *host, cchar *field, cchar *defaultValue)
{
    uint64 value;

    value = svalue(jsonGet(host->config, 0, field, defaultValue));
    if (value > MAXINT / 1000) {
        return MAXINT;
    }
    return (int) value * 1000;
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
        method = strim(method, "\"", R_TRIM_BOTH);
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
    JsonNode *child, *mime;
    cchar    **mp;
    int      id;

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
    mime = jsonGetNode(host->config, 0, "web.mime");
    if (mime) {
        for (ITERATE_JSON(host->config, mime, child, id)) {
            rAddName(host->mimeTypes, child->name, child->value, 0);
        }
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
    int      id;

    host->routes = rAllocList(0, 0);
    json = host->config;
    routes = jsonGetNode(json, 0, "web.routes");

    if (routes == 0) {
        rp = rAllocType(WebRoute);
        rp->match = sclone("/");
        rAddItem(host->routes, rp);

    } else {
        for (ITERATE_JSON(json, routes, route, id)) {
            match = jsonGet(json, id, "match", 0);
            rp = rAllocType(WebRoute);
            rp->match = sclone(match);

            /*
                Exact match if pattern non-empty and not a trailing "/"
                Empty routes match everything
                A match of "/" will do an exact match.
             */
            rp->exact = (!match || slen(match) == 0 ||
                         (slen(match) > 0 && match[slen(match) - 1] == '/' && !smatch(match, "/"))
                         ) ? 0 : 1;
            rp->role = jsonGet(json, id, "role", 0);
            rp->redirect = jsonGet(json, id, "redirect", 0);
            rp->trim = jsonGet(json, id, "trim", 0);
            rp->handler = jsonGet(json, id, "handler", "file");
            rp->stream = jsonGetBool(json, id, "stream", 0);

            if ((methods = jsonToString(json, id, "methods", 0)) != 0) {
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
    int         id, status;

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


/********* Start of file src/io.c ************/

/*
    io.c - I/O for the web server

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



/************************************ Forwards *********************************/

static char *findPattern(RBuf *buf, cchar *pattern);
static RHash *getTxHeaders(Web *web);
static bool isprintable(cchar *s, ssize len);
static ssize readChunk(Web *web, char *buf, ssize bufsize);
static ssize readWeb(Web *web, char *buf, ssize bufsize);
static int writeChunk(Web *web, ssize bufsize);

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
        nbytes = readWeb(web, buf, bufsize);
    }
    if (nbytes < 0) {
        if (web->rxRemaining > 0) {
            return webNetError(web, "Cannot read from socket");
        }
        web->close = 1;
        return 0;
    }
    web->rxRemaining -= nbytes;
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
        chunkSize = (int) stoiradix(cbuf, 16, NULL);
        if (chunkSize < 0) {
            return webNetError(web, "Bad chunk specification");

        } else if (chunkSize) {
            web->chunkRemaining = chunkSize;
            web->chunked = WEB_CHUNK_DATA;
            // web->rxRemaining += chunkSize;

        } else {
            //  Zero chunk -- end of body
            if (webReadUntil(web, "\r\n", cbuf, sizeof(cbuf)) < 0) {
                return webNetError(web, "Bad chunk data");
            }
            web->chunkRemaining = 0;
            web->rxRemaining = 0;
        }
    }
    if (web->chunked == WEB_CHUNK_DATA) {
        if ((nbytes = readWeb(web, buf, min(bufsize, web->chunkRemaining))) < 0) {
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
    Low-level read data from the socket into the web->rx buffer.
 */
static ssize readWeb(Web *web, char *buf, ssize bufsize)
{
    RBuf  *bp;
    ssize nbytes, toRead;

    bp = web->rx;

    if (rGetBufLength(bp) == 0 && bufsize > 0) {
        rCompactBuf(bp);
        rReserveBufSpace(bp, ME_BUFSIZE);
        toRead = rGetBufSpace(bp);
        if ((nbytes = rReadSocket(web->sock, bp->end, toRead, web->deadline)) < 0) {
            return webNetError(web, "Cannot read from socket");
        }
        rAdjustBufEnd(bp, nbytes);
    }
    nbytes = min(rGetBufLength(bp), bufsize);
    if (nbytes) {
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
    cp = buf->start;
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
PUBLIC int webWriteHeaders(Web *web)
{
    RName *header;
    RBuf  *buf;
    char  date[32];
    int   status;

    buf = web->tx;
    status = web->status;
    if (status == 0) {
        status = 500;
    }
    if (web->creatingHeaders) {
        return 0;
    }
    web->creatingHeaders = 1;

    if (web->wroteHeaders) {
        rError("web", "Headers already created");
        return 0;
    }

    webAddHeader(web, "Date", webDate(date, time(0)));
    webAddHeaderStaticString(web, "Connection", web->close ? "close" : "keep-alive");

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
    if (web->txLen >= 0) {
        //  Delay adding if using transfer encoding. This optimization eliminates a write per chunk.
        rPutStringToBuf(buf, "\r\n");
    }

    if (webWrite(web, rGetBufStart(buf), rGetBufLength(buf)) < 0) {
        return R_ERR_CANT_WRITE;
    }
    web->creatingHeaders = 0;
    web->wroteHeaders = 1;

    rFlushBuf(buf);
    return 0;
}

/*
    Add headers from web.json
 */
PUBLIC void webAddStandardHeaders(Web *web)
{
    WebHost  *host;
    Json     *json;
    JsonNode *headers, *header;
    int      id;

    host = web->host;
    if (host->headers >= 0) {
        json = host->config;
        headers = jsonGetNode(json, host->headers, 0);
        for (ITERATE_JSON(json, headers, header, id)) {
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
    rAddName(getTxHeaders(web), key, value, R_DYNAMIC_VALUE);
}

PUBLIC void webAddHeaderStaticString(Web *web, cchar *key, cchar *value)
{
    rAddName(getTxHeaders(web), key, (void*) value, R_STATIC_VALUE);
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
    Write body data. Set bufsize to zero to signify end of body if the content length is not defined.
    Will close the socket on socket errors
 */
PUBLIC ssize webWrite(Web *web, cvoid *buf, ssize bufsize)
{
    ssize written;

    if (bufsize < 0) {
        bufsize = slen(buf);
    }
    if (web->complete) {
        if (bufsize) {
            rError("web", "Writing too much data");
            return R_ERR_BAD_STATE;
        }
        return 0;
    }
    if (!web->wroteHeaders && webWriteHeaders(web) < 0) {
        //  Already closed
        return R_ERR_CANT_WRITE;
    }
    if (writeChunk(web, bufsize) < 0) {
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
    if (web->txRemaining <= 0) {
        web->complete = 1;
    }
    webUpdateDeadline(web);
    return written;
}

/*
    Finalize output for this request
 */
PUBLIC ssize webFinalize(Web *web)
{
    return webWrite(web, 0, 0);
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

/*
    Write all or a portion of a json object
 */
PUBLIC ssize webWriteJson(Web *web, Json *json, int nid, cchar *key)
{
    char  *str;
    ssize rc;

    str = jsonToString(json, nid, key, JSON_STRICT);
    rc = webWrite(web, str, -1);
    rFree(str);
    return rc;
}

/*
    Write a transfer-chunk encoded divider
 */
static int writeChunk(Web *web, ssize size)
{
    char chunk[24];

    if (web->txLen >= 0 || !web->wroteHeaders) {
        return 0;
    }
    if (size == 0) {
        scopy(chunk, sizeof(chunk), "\r\n0\r\n\r\n");
        web->complete = 1;
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
 */
PUBLIC int webWriteResponse(Web *web, int status, cchar *fmt, ...)
{
    va_list ap;
    char    *msg;
    int     rc;

    if (!fmt) {
        fmt = "";
    }
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
    if (status) {
        web->status = status;
    }
    webAddHeaderStaticString(web, "Content-Type", "text/plain");

    rc = 0;
    if (webWriteHeaders(web) < 0) {
        rc = R_ERR_CANT_WRITE;
    } else if (web->status != 204 && !smatch(web->method, "HEAD")) {
        if (webWrite(web, msg, web->txLen) < 0) {
            rc = R_ERR_CANT_WRITE;
        }
    }
    webFinalize(web);
    if (status != 200 && status != 204 && status != 301 && status != 302 && status != 401) {
        rTrace("web", "%s", msg);
    }
    if (!web->error) {
        rFree(msg);
    }
    return rc;
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

    if ((port == 80 && smatch(scheme, "http")) || (port == 443 && smatch(scheme, "https"))) {
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
    rFree(uri);

    webWriteResponse(web, status, NULL);
    rFree(freeHost);
    rFree(buf);
}

/*
    Issue a request error response.
    If status is zero, close the socket after trying to issue a response.
    Otherwise, the socket and connection remain usable for further requests.
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
    return 0;
}

/*
    Close the socket and issue no response. The connection is not usable.
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


/********* Start of file src/session.c ************/

/*
    session.c - User session state control

    This implements server side request state that is identified by a request cookie.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



/************************************ Locals **********************************/
#if ME_WEB_SESSIONS

#define WEB_SESSION_PRUNE (60 * 1000)      /* Prune sessions every minute */

/*********************************** Forwards *********************************/

static WebSession *createSession(Web *web);
static char *makeSessionID(Web *web);
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
    sp->id = makeSessionID(web);

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
        id = webParseCookie(web, WEB_SESSION_COOKIE);
        if ((session = rLookupName(web->host->sessions, id)) == 0) {
            if (create) {
                session = createSession(web);
            }
        }
        rFree(id);
    }
    if (session) {
        session->expires = rGetTicks() + session->lifespan;
        web->session = session;
    }
    return session;
}

static WebSession *createSession(Web *web)
{
    WebSession *session;
    WebHost    *host;
    cchar      *httpOnly, *secure;
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
    secure = rIsSocketSecure(web->sock) ? "Secure; " : "";
    httpOnly = host->httpOnly ? "HttpOnly; " : "";
    webAddHeader(web, "Set-Cookie", "%s=%s; path=/; %s%sSameSite=%s", WEB_SESSION_COOKIE, session->id, secure,
                 httpOnly, host->sameSite);
    return session;
}

PUBLIC char *webParseCookie(Web *web, char *name)
{
    char *buf, *cookie, *end, *key, *tok, *value, *vtok;

    assert(web);

    if (web->cookie == 0 || name == 0 || *name == '\0') {
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

static char *makeSessionID(Web *web)
{
    static int nextSession = 0;
    char       idBuf[64];

    assert(web);
    sfmtbuf(idBuf, sizeof(idBuf), ":web.session:%08x%08x%d", PTOI(web) + PTOI(web->url),
            (int) rGetTicks(), nextSession++);
    return cryptGetSha256((cuchar*) idBuf, slen(idBuf));
}
#endif /* ME_WEB_SESSION */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/test.c ************/

/*
    test.c - Test routines for debug mode only

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
    char     *body, keybuf[80], *result;
    cchar    *key, *value;
    ssize    len;
    bool     isPrintable;
    int      i, nid;

    json = jsonAlloc(0);
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
        for (ITERATE_JSON(web->vars, NULL, node, nid)) {
            jsonSetFmt(json, 0, SFMT(keybuf, "form.%s", node->name), "%s", node->value);
        }
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
    if (rGetBufLength(web->body)) {
        len = rGetBufLength(web->body);
        body = rGetBufStart(web->body);
        jsonSetFmt(json, 0, "bodyLength", "%ld", len);
        isPrintable = 1;
        for (i = 0; i < len; i++) {
            if (!isprint((uchar) body[i]) || body[i] == '\n' || body[i] == '\r' || body[i] == '\t') {
                isPrintable = 0;
                break;
            }
        }
        if (isPrintable) {
            body = sreplace(rBufToString(web->body), "\"", "\\\"");
            jsonSetFmt(json, 0, "body", "%s", body);
            rFree(body);
        }
    }
    showRequestContext(web, json);
    showServerContext(web, json);
    result = jsonToString(json, 0, NULL, JSON_STRICT | JSON_PRETTY);
    webWrite(web, result, slen(result));
    rFree(result);
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

static void formAction(Web *web)
{
    webAddHeaderStaticString(web, "Cache-Control", "no-cache");
    webWriteFmt(web, "<html><head><title>form.esp</title></head>\n");
    webWriteFmt(web, "<body><form name='details' method='post' action='form'>\n");
    webWriteFmt(web, "Name <input type='text' name='name' value='%s'>\n", webGetVar(web, "name", ""));
    webWriteFmt(web, "Address <input type='text' name='address' value='%s>\n", webGetVar(web, "address", ""));
    webWriteFmt(web, "<input type='submit' name='submit' value='OK'></form>\n\n");
    webWriteFmt(web, "<h3>Request Details</h3>\n\n");
    showRequest(web);
    webWriteFmt(web, "</pre>\n</body>\n</html>\n");
    webFinalize(web);
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

static void uploadAction(Web *web)
{
    showRequest(web);
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

PUBLIC void webTestInit(WebHost *host, cchar *prefix)
{
    char url[128];

    webAddAction(host, SFMT(url, "%s/form", prefix), formAction, NULL);
    webAddAction(host, SFMT(url, "%s/bulk", prefix), bulkOutput, NULL);
    webAddAction(host, SFMT(url, "%s/error", prefix), errorAction, NULL);
    webAddAction(host, SFMT(url, "%s/success", prefix), successAction, NULL);
    webAddAction(host, SFMT(url, "%s/show", prefix), showAction, NULL);
    webAddAction(host, SFMT(url, "%s/upload", prefix), uploadAction, NULL);
    webAddAction(host, SFMT(url, "%s/stream", prefix), streamAction, NULL);
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


/********* Start of file src/upload.c ************/

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
    //  Freed in freeWebFields (web.c)
    web->vars = jsonAlloc(0);
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
                    filename = webNormalizePath(value);
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
        if (*path == '.' || !webValidatePath(path) || strpbrk(path, "\\/:*?<>|~\"'%`^\n\r\t\f")) {
            webError(web, 400, "Bad upload client filename");
            return 0;
        }
        upload->clientFilename = sclone(path);

        /*
            Create the file to hold the uploaded data
         */
        if ((upload->filename = rGetTempFile(web->host->uploadDir, "tmp")) == 0) {
            webError(web, 500, "Cannot create upload temp file %s. Check upload temp dir %s", upload->filename,
                     web->host->uploadDir);
            return 0;
        }
        rTrace("web", "File upload of: %s stored as %s", upload->clientFilename, upload->filename);

        if ((upload->fd = open(upload->filename, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0600)) < 0) {
            webError(web, 500, "Cannot open upload temp file %s", upload->filename);
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
                If webBufferUntil returned 0 (short), then the boundary was not seen.
                In this case, write data and continue but preserve 2 bytes incase it is the \r\n before the boundary.

                If the boundary is found, preserve it (and the \r\n) to be consumed by webProcessUpload.
             */
            len = (nbytes ? nbytes - web->boundaryLen : rGetBufLength(web->rx)) - 2;

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


/********* Start of file src/utils.c ************/

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

    len = slen(str);
    for (ip = op = str; *ip && len > 0; ip++, op++) {
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
    char *buf, *next, *tok;

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
    if (!schr(tok, '/') && (smatch(tok, "http") || smatch(tok, "https"))) {
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
            if (smatch(tok, "https")) {
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
                        *port = atoi(tok);
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
    Normalize a path to remove "./",  "../" and redundant separators. This does not make an abs path
    and does not map separators nor change case. This validates the path and expects it to begin with "/".
    Returns an allocated path, caller must free.
 */
PUBLIC char *webNormalizePath(cchar *pathArg)
{
    char *dupPath, *path, *sp, *dp, *mark, **segments;
    int  firstc, j, i, nseg, len;

    if (pathArg == 0 || *pathArg == '\0') {
        return 0;
    }
    len = (int) slen(pathArg);
    if ((dupPath = rAlloc(len + 2)) == 0) {
        return 0;
    }
    strcpy(dupPath, pathArg);

    if ((segments = rAlloc(sizeof(char*) * (len + 1))) == 0) {
        rFree(dupPath);
        return NULL;
    }
    nseg = len = 0;
    firstc = *dupPath;
    for (mark = sp = dupPath; *sp; sp++) {
        if (*sp == '/') {
            *sp = '\0';
            while (sp[1] == '/') {
                sp++;
            }
            segments[nseg++] = mark;
            len += (int) (sp - mark);
            mark = sp + 1;
        }
    }
    segments[nseg++] = mark;
    len += (int) (sp - mark);
    for (j = i = 0; i < nseg; i++, j++) {
        sp = segments[i];
        if (sp[0] == '.') {
            if (sp[1] == '\0') {
                if ((i + 1) == nseg) {
                    // Trim trailing "."
                    segments[j] = "";
                } else {
                    j--;
                }
            } else if (sp[1] == '.' && sp[2] == '\0') {
                j = max(j - 2, -1);
                if ((i + 1) == nseg) {
                    nseg--;
                }
            } else {
                // more-chars
                segments[j] = segments[i];
            }
        } else {
            segments[j] = segments[i];
        }
    }
    nseg = j;
    assert(nseg >= 0);
    if ((path = rAlloc(len + nseg + 1)) != 0) {
        for (i = 0, dp = path; i < nseg; ) {
            strcpy(dp, segments[i]);
            len = (int) slen(segments[i]);
            dp += len;
            if (++i < nseg || (nseg == 1 && *segments[0] == '\0' && firstc == '/')) {
                *dp++ = '/';
            }
        }
        *dp = '\0';
    }
    rFree(dupPath);
    rFree(segments);
    return path;
}

/*
    Escape HTML to escape defined characters (prevent cross-site scripting). Returns an allocated string.
 */
PUBLIC char *webEscapeHtml(cchar *html)
{
    cchar *ip;
    char  *result, *op;
    int   len;

    if (!html) {
        return sclone("");
    }
    for (len = 1, ip = html; *ip; ip++, len++) {
        if (charMatch[(int) (uchar) * ip] & WEB_ENCODE_HTML) {
            len += 5;
        }
    }
    if ((result = rAlloc(len)) == 0) {
        return 0;
    }
    /*
        Leave room for the biggest expansion
     */
    op = result;
    while (*html != '\0') {
        if (charMatch[(uchar) * html] & WEB_ENCODE_HTML) {
            if (*html == '&') {
                strcpy(op, "&amp;");
                op += 5;
            } else if (*html == '<') {
                strcpy(op, "&lt;");
                op += 4;
            } else if (*html == '>') {
                strcpy(op, "&gt;");
                op += 4;
            } else if (*html == '#') {
                strcpy(op, "&#35;");
                op += 5;
            } else if (*html == '(') {
                strcpy(op, "&#40;");
                op += 5;
            } else if (*html == ')') {
                strcpy(op, "&#41;");
                op += 5;
            } else if (*html == '"') {
                strcpy(op, "&quot;");
                op += 6;
            } else if (*html == '\'') {
                strcpy(op, "&#39;");
                op += 5;
            } else {
                assert(0);
            }
            html++;
        } else {
            *op++ = *html++;
        }
    }
    assert(op < &result[len]);
    *op = '\0';
    return result;
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

    assert(uri);

    if (!uri) {
        return sclone("");
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


/********* Start of file src/web.c ************/

/*
    web.c -

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



/************************************ Forwards *********************************/

static void freeWebFields(Web *web, bool keepAlive);
static void initWeb(Web *web, WebListen *listen, RSocket *sock, RBuf *rx, int close);
static int handleRequest(Web *web);
static bool matchFrom(Web *web, cchar *from);
static int parseHeaders(Web *web, ssize headerSize);
static int parseUrl(Web *web);
static int processBody(Web *web);
static void processOptions(Web *web);
static void processRequests(Web *web);
static void processQuery(Web *web);
static int readBody(Web *web);
static int redirectRequest(Web *web);
static void resetWeb(Web *web);
static bool routeRequest(Web *web);
static int serveRequest(Web *web);
static int webActionHandler(Web *web);
static int validateUrl(Web *web);

/************************************ Locals **********************************/

static int64 conn = 0; /* Connection sequence number */

/************************************* Code ***********************************/

PUBLIC int webAlloc(WebListen *listen, RSocket *sock)
{
    Web     *web;
    WebHost *host;

    assert(!rIsMain());

    host = listen->host;

    if (++host->connections > host->maxConnections) {
        rTrace("web", "Too many connections %d/%d", (int) host->connections, (int) host->maxConnections);
        rFreeSocket(sock);
        return R_ERR_WONT_FIT;
    }
    if ((web = rAllocType(Web)) == 0) {
        return R_ERR_MEMORY;
    }
    web->conn = conn++;
    initWeb(web, listen, sock, 0, 0);
    rAddItem(host->webs, web);

    webHook(web, WEB_HOOK_CONNECT);

    processRequests(web);

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

    web->body = rAllocBuf(ME_BUFSIZE);
    web->rx = rx ? rx : rAllocBuf(ME_BUFSIZE);
    web->rxHeaders = rAllocBuf(ME_BUFSIZE);
    web->tx = rAllocBuf(ME_BUFSIZE);
    web->status = 200;
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
    rFree(web->path);
    rFree(web->error);
    rFree(web->redirect);
    rFree(web->cookie);
    rFreeBuf(web->body);
    rFreeBuf(web->tx);
    rFreeBuf(web->trace);
    rFreeBuf(web->rxHeaders);
    rFreeHash(web->txHeaders);

    jsonFree(web->vars);
    jsonFree(web->qvars);
    webFreeUpload(web);

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
        return R_ERR_CANT_READ;
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

    if (web->complete) {
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

    if (tolower(web->method[0]) == 'o' && scaselessmatch(web->method, "OPTIONS") && route->methods) {
        processOptions(web);
        return 0;
    }
    if (web->uploads && webProcessUpload(web) < 0) {
        return 0;
    }
    if (web->query) {
        processQuery(web);
    }
    if (!route->stream && (web->rxRemaining > 0 || web->chunked)) {
        if (readBody(web) < 0) {
            return R_ERR_CANT_COMPLETE;
        }
        if (processBody(web) < 0) {
            return 0;
        }
    }
    webUpdateDeadline(web);

    /*
        Request ready to run, allow any request modification or running a custom handler
     */
    webHook(web, WEB_HOOK_RUN);
    if (web->complete) {
        return 0;
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

static int webActionHandler(Web *web)
{
    WebAction *action;
    int       next;

    for (ITERATE_ITEMS(web->host->actions, action, next)) {
        if (sstarts(web->path, action->match)) {
            if (!action->role || webCan(web, action->role)) {
                webHook(web, WEB_HOOK_ACTION);
                (action->fn)(web);
                return 0;
            }
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

            } else if (route->role) {
                if (!webAuthenticate(web) || !webCan(web, route->role)) {
                    webError(web, 401, "Access Denied. User not logged in or insufficient privilege.");
                    return 0;
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
    if (!web->complete) {
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
    if (path && !smatch(&web->path[1], path)) {
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
    if (validateUrl(web) < 0) {
        return R_ERR_BAD_REQUEST;
    }
    webUpdateDeadline(web);
    return 0;
}

/*
    Parse a headers block. Used here and by file upload.
 */
PUBLIC bool webParseHeadersBlock(Web *web, char *headers, ssize headersSize, bool upload)
{
    struct tm tm;
    cchar     *end;
    char      c, *cp, *key, *prior, *value;

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
        *cp++ = '\0';
        while (*cp && *cp == ' ') cp++;
        value = cp;
        while (*cp && *cp != '\r') cp++;

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
                web->rxLen = web->rxRemaining = atoi(value);

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

        } else if (c == 'o' && scaselessmatch(key, "origin")) {
            web->origin = value;

        } else if (c == 't' && scaselessmatch(key, "transfer-encoding")) {
            web->chunked = WEB_CHUNK_START;
        }
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
            while (*cp == ' ') cp++;
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
        while (*cp == ' ') cp++;
        *pvalue = cp;
        return 1;
    }
    return 0;
}

/*
    Validate the request URL
 */
static int validateUrl(Web *web)
{
    if (web->url == 0 || *web->url == 0) {
        return webNetError(web, "Empty URL");
    }
    if (!webValidatePath(web->url)) {
        webNetError(web, "Bad characters in URL");
        return R_ERR_BAD_ARGS;
    }
    webDecode(web->url);

    if (parseUrl(web) < 0) {
        //  Already set error
        return R_ERR_BAD_ARGS;
    }
    return 0;
}

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
                web->ext = tok;
            }
        }
    }
    if ((web->path = webNormalizePath(path)) == 0) {
        return webNetError(web, "Illegal URL");
    }
    return 0;
}

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
    Read body data from the rx buffer into the body buffer
    Not called for streamed requests
 */
static int readBody(Web *web)
{
    ssize nbytes;
    RBuf  *buf;

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
    return 0;
}

/*
    Process the request body and parse JSON, url-encoded forms and query vars.
 */
static int processBody(Web *web)
{
    if (web->host->flags & WEB_SHOW_REQ_BODY && rGetBufLength(web->body)) {
        rLog("raw", "web", "Request Body <<<<\n\n%s\n\n", rBufToString(web->body));
    }
    if (web->jsonBody) {
        if ((web->vars = webParseJson(web)) == 0) {
            return webError(web, 400, "JSON body is malformed");
        }
    } else if (web->formBody) {
        web->vars = jsonAlloc(0);
        webParseForm(web);
    }
    return 0;
}

static void processQuery(Web *web)
{
    web->qvars = jsonAlloc(0);
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

PUBLIC void webExtendTimeout(Web *web, Ticks timeout)
{
    web->deadline = rGetTimeouts() ? rGetTicks() + timeout : 0;
}

PUBLIC int webHook(Web *web, int event)
{
    if (web->host->hook) {
        return (web->host->hook)(web, event);
    }
    return 0;
}

PUBLIC void webUpdateDeadline(Web *web)
{
    web->deadline = rGetTimeouts() ?
                    min(rGetTicks() + web->host->inactivityTimeout, web->started + web->host->requestTimeout) : 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */

#else
void dummyWeb(){}
#endif /* ME_COM_WEB */
