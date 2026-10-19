/*
    webLib.c -- Web Library Source

    This file is a catenation of all the source code. Amalgamating into a
    single file makes embedding simpler and the resulting application faster,
    by using compiler optimization within the Web library.

    Prepared by: buildLib.sh
 */

#include "web.h"

#if ME_COM_WEB


/********* Start of file src/auth.c ************/

/*
    auth.c -- Authorization Management

    This module supports a general user authentication scheme.
    It support web-form based login and HTTP Basic and Digest authentication.
    Users with role/ability based authorization is supported.

    In this module, Users have passwords and roles. A role grants abilities (permissions) to perform actions.
    Roles can inherit from other roles, creating a hierarchy of permissions.

    Three authentication protocols are supported:
        - HTTP Basic authentication (RFC 7617) - Base64 encoded username:password
        - HTTP Digest authentication (RFC 7616/2617) - Challenge-response with MD5 or SHA-256
        - Session-based authentication - Cached authentication state in sessions

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "web.h"

/********************************** Forwards **********************************/

static int computeUserAbilities(WebHost *host, WebUser *user);
static int expandRole(WebHost *host, cchar *roleName, RHash *abilities);

#if ME_WEB_HTTP_AUTH
static void sendAuthChallenge(Web *web, WebRoute *route);
static WebUser *lookupHttpUser(Web *web);
#if ME_WEB_AUTH_BASIC
static bool parseBasicAuth(Web *web);
static bool verifyPassword(Web *web, WebUser *user);
static void sendBasicChallenge(Web *web);
#endif
#if ME_WEB_AUTH_DIGEST
static bool parseDigestAuth(Web *web);
static char *computeDigest(Web *web, cchar *password);
static char *createNonce(Web *web, cchar *algorithm);
static bool matchDigestUri(Web *web, cchar *uri);
static bool validateNonce(Web *web);
static void sendDigestChallenge(Web *web, WebRoute *route);
static void cleanupNonces(void *arg);
static void pruneNonces(WebHost *host);
static ssize nonceUserCount(WebHost *host, cchar *username);
static void addNonceUser(WebHost *host, cchar *username, ssize delta);
static Ticks getNonceCleanupPeriod(WebHost *host);
static bool commitNonce(Web *web);
static bool parseNc(cchar *nc, uint64 *value);
#endif
#endif /* ME_WEB_HTTP_AUTH */

/************************************ Code ************************************/

/*
    Authenticate the current request.
    This checks if the request has a current session by using the request cookie.
    Returns true if authenticated. Residual: set web->authenticated.
 */
PUBLIC bool webAuthenticate(Web *web)
{
    WebUser *user;
    cchar   *username, *role;

    if (web->authChecked) {
        return web->authenticated;
    }
    web->authChecked = 1;

    if (web->cookie && webGetSession(web, 0) != 0) {
        /*
            SECURITY Acceptable:: Retrieve authentication state from the session storage.
            Faster than re-authenticating.
         */
        username = webGetSessionVar(web, WEB_SESSION_USERNAME, 0);
        role = webGetSessionVar(web, WEB_SESSION_ROLE, 0);
        if (username && role) {
            //  Look up user from session username
            if ((user = webLookupUser(web->host, username)) != 0) {
                //  Verify user still has the cached role. Only a verified identity is published.
                if (smatch(user->role, role)) {
                    rFree(web->username);
                    web->username = sclone(username);
                    rFree(web->role);
                    web->role = sclone(role);
                    web->user = user;
                    web->authenticated = 1;
                    return 1;
                } else {
                    rError("web", "User %s role changed from %s to %s", username, role, user->role);
                }
            } else {
                rError("web", "Unknown user in session: %s", username);
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
    Check if user has required ability
 */
PUBLIC bool webUserCan(WebUser *user, cchar *ability)
{
    if (!user || !ability) {
        return 0;
    }
    // Check specific ability first (common case) for better performance
    if (rLookupName(user->abilities, ability)) {
        return 1;
    }
    // Wildcard ability grants everything (rare case)
    return rLookupName(user->abilities, "*") != 0;
}

/*
    Check if the authenticated user has the required ability/role
    Uses the new ability-based authorization system
 */
PUBLIC bool webCan(Web *web, cchar *requiredRole)
{
    assert(web);

    if (!requiredRole || *requiredRole == '\0' || smatch(requiredRole, "public")) {
        return 1;
    }
    if (!web->authenticated && !webAuthenticate(web)) {
        return 0;
    }
    //  Use the new ability-based system
    if (!web->user) {
        return 0;
    }
    return webUserCan(web->user, requiredRole);
}

/*
    Return the role of the authenticated user
 */
PUBLIC cchar *webGetRole(Web *web)
{
    if (!web->authenticated || !web->user) {
        return 0;
    }
    return web->user->role;
}

/*
    Create a login session for a caller-verified user. Role is a role/ability to check, or the current role for
    application-managed credentials.
 */
PUBLIC bool webLogin(Web *web, cchar *username, cchar *role)
{
    WebUser *user;

    assert(web);
    assert(username);
    assert(role);

    rFree(web->username);
    web->username = 0;
    rFree(web->role);
    web->role = 0;
    web->user = 0;
    web->authenticated = 0;

    webRemoveSessionVar(web, WEB_SESSION_USERNAME);

    if ((user = webLookupUser(web->host, username)) == 0) {
        // This is used by users that manage their own users (via database)
        user = webAddUser(web->host, username, NULL, role);
        if (!user) {
            rError("web", "Failed to add user %s", username);
            return 0;
        }
    } else if (!webUserHasCredential(user) && !smatch(user->role, role)) {
        /* Refresh application-managed roles so existing sessions with outdated roles fail authentication. */
        if (!webUpdateUser(web->host, username, NULL, role)) {
            rError("web", "Cannot bind user %s to role %s", username, role);
            return 0;
        }
    }
    //  Verify user has the required ability/role
    if (!webUserCan(user, role)) {
        rError("web", "User %s does not have ability %s", username, role);
        return 0;
    }
    if (!webCreateSession(web)) {
        return 0;
    }
    webSetSessionVar(web, WEB_SESSION_USERNAME, "%s", username);
    web->username = sclone(username);
    rFree(web->role);
    web->role = sclone(webSetSessionVar(web, WEB_SESSION_ROLE, "%s", user->role));
    web->user = user;
    web->authenticated = 1;
    return 1;
}

/*
    Logout the authenticated user by destroying the user session
 */
PUBLIC void webLogout(Web *web)
{
    assert(web);

    rFree(web->username);
    web->username = 0;
    rFree(web->role);
    web->role = 0;
    web->user = 0;
    web->authenticated = 0;
    webRemoveSessionVar(web, WEB_SESSION_USERNAME);
    webDestroySession(web);
}
/*
    Lookup user by username
 */
PUBLIC WebUser *webLookupUser(WebHost *host, cchar *username)
{
    if (!host || !username) {
        return 0;
    }
    return (WebUser*) rLookupName(host->users, username);
}

/*
    Add a user with a pre-hashed H(username:realm:password) credential and one role. Null/empty passwords select
    application-managed authentication.
 */
PUBLIC WebUser *webAddUser(WebHost *host, cchar *username, cchar *password, cchar *role)
{
    WebUser *user;

    if (!host || !username || !role) {
        return 0;
    }
    if (slen(password) > ME_WEB_MAX_AUTH) {
        rError("web", "Password too long");
        return 0;
    }
    if (slen(username) > ME_WEB_MAX_AUTH) {
        rError("web", "Username too long");
        return 0;
    }
    if (webLookupUser(host, username)) {
        rError("web", "User %s already exists", username);
        return 0;
    }
    user = rAllocType(WebUser);
    user->username = sclone(username);
    user->password = password && *password ? sclone(password) : 0;
    user->role = sclone(role);
    user->abilities = rAllocHash(0, 0);

    //  Compute abilities from role hierarchy
    if (computeUserAbilities(host, user) < 0) {
        webFreeUser(user);
        return NULL;
    }
    rAddName(host->users, username, user, R_TEMPORAL_NAME);
    return user;
}

/*
    Remove user from authentication database
 */
/*
    Invalidate any in-flight request that borrows this user.

    web->user is a borrowed pointer into host->users with no ownership and no invalidation. Ioto is
    cooperatively scheduled, so a request fiber blocked on I/O resumes to find whatever the user
    table now holds - and webRemoveUser, webUpdateUser and webAddUser can free the entry while it
    is blocked. Clearing the borrowed pointer turns a use-after-free into a failed authorization,
    which is the safe direction.
 */
static void invalidateUserRefs(WebHost *host, WebUser *user)
{
    Web *web;
    int next;

    if (!host || !host->webs) {
        return;
    }
    for (ITERATE_ITEMS(host->webs, web, next)) {
        if (web->user == user) {
            web->user = 0;
            web->authenticated = 0;
        }
    }
}

PUBLIC bool webRemoveUser(WebHost *host, cchar *username)
{
    WebUser *user;

    if (!host || !username) {
        return 0;
    }
    if ((user = webLookupUser(host, username)) != 0) {
        /*
            Unhook before freeing. The natural call is webRemoveUser(host, user->username), and
            webFreeUser releases that string - so removing first, on a key that outlives the free,
            is the only ordering safe for every caller.
         */
        char *key = sclone(username);
        invalidateUserRefs(host, user);
        webDestroyUserSessions(host, key);
        rRemoveName(host->users, key);
        webFreeUser(user);
        rFree(key);
        return 1;
    }
    return 0;
}

/*
    Update user's password and role
    Password should be pre-hashed: H(username:realm:password)
 */
PUBLIC bool webUpdateUser(WebHost *host, cchar *username, cchar *password, cchar *role)
{
    WebUser *user;
    char    *value;

    if (!host || !username) {
        return 0;
    }
    if (slen(password) > ME_WEB_MAX_AUTH) {
        return 0;
    }
    if (slen(username) > ME_WEB_MAX_AUTH) {
        return 0;
    }
    if ((user = webLookupUser(host, username)) == 0) {
        return 0;
    }
    if (password) {
        //  Clone before freeing: the caller may pass the stored value back
        value = sclone(password);
        if (!smatch(value, user->password)) {
            webDestroyUserSessions(host, username);
        }
        rFree(user->password);
        user->password = value;
    }
    if (role) {
        value = sclone(role);
        rFree(user->role);
        user->role = value;
        rFreeHash(user->abilities);   //  An RHash - rFree would leak the table
        user->abilities = rAllocHash(0, 0);
        if (computeUserAbilities(host, user) < 0) {
            // User downgraded with no abilities and new role
            return 0;
        }
    }
    return 1;
}

/*
    Free user structure
 */
PUBLIC void webFreeUser(WebUser *user)
{
    if (user) {
        rFree(user->username);
        rFree(user->password);
        rFree(user->role);
        rFreeHash(user->abilities);   //  An RHash - rFree would leak the table
        rFree(user);
    }
}

/*
    Application-managed credentials have no stored password.
 */
PUBLIC bool webUserHasCredential(WebUser *user)
{
    return user && user->password && *user->password;
}

/*
    Compute user abilities from role hierarchy
    Recursively expands roles to include inherited abilities
    Supports both legacy array format and new object format
 */
static int computeUserAbilities(WebHost *host, WebUser *user)
{
    JsonNode *rolesNode, *child;
    Json     *config;
    int      roleIndex, i;

    if (!host || host->roles < 0) {
        rError("web", "Cannot compute user abilities. Missing auth roles.");
        return R_ERR_BAD_ARGS;
    }
    if (!user || !user->role) {
        rError("web", "Cannot compute user abilities. Missing user or user role.");
        return R_ERR_BAD_ARGS;
    }
    config = host->config;
    rolesNode = jsonGetNode(config, host->roles, NULL);
    if (!rolesNode) {
        rError("web", "Cannot auth roles");
        return R_ERR_BAD_ARGS;
    }
    /*
        Handle legacy array format: roles: ['user', 'admin', 'owner', 'super']
        Array format creates a hierarchy where each role inherits from all previous roles
        Equivalent to: { public: [], user: ['public'], admin: ['user'], owner: ['admin'], super: ['owner'] }
     */
    if (rolesNode->type == JSON_ARRAY) {
        rDebug("web", "Legacy array format detected, please convert to object format");
        roleIndex = -1;
        i = 0;
        // Find the user's role position in the array
        for (ITERATE_JSON_ID(config, host->roles, child, childId)) {
            if (child->value && smatch(child->value, user->role)) {
                roleIndex = i;
                break;
            }
            i++;
        }
        if (roleIndex < 0) {
            rError("web", "Cannot find role %s in roles array", user->role);
            return R_ERR_CANT_FIND;
        }
        // Add 'public' as a base ability (implicit for all roles)
        rAddName(user->abilities, "public", (void*) 1, 0);

        /*
            Add all roles from start up to and including the user's role as abilities
            This creates inheritance: 'owner' gets abilities: public, user, admin, owner
         */
        i = 0;
        for (ITERATE_JSON_ID(config, host->roles, child, childId)) {
            if (child->value) {
                rAddName(user->abilities, child->value, (void*) 1, 0);
            }
            if (i >= roleIndex) {
                break;
            }
            i++;
        }
        return 0;
    }
    // Handle new object format with role inheritance
    return expandRole(host, user->role, user->abilities);
}

/*
    Recursively expand a role to compute all abilities
    Handles both direct abilities and role inheritance
 */
static int expandRole(WebHost *host, cchar *roleName, RHash *abilities)
{
    Json     *config;
    JsonNode *child;
    cchar    *item;
    int      roleId;

    if (!roleName) {
        return R_ERR_BAD_ARGS;
    }
    if (rLookupName(abilities, roleName)) {
        //  Already visited
        return 0;
    }
    config = host->config;

    /*
        Validate the role exists BEFORE granting it. Adding roleName to the abilities set first meant
        that when the name did not resolve, expandRole still returned failure but left the unverified
        name behind as a granted ability; webUpdateUser does not roll back on failure, so a role update
        reported as failed could silently leave the user holding an attacker/caller-chosen ability.
        Adding only after the lookup succeeds still marks the role visited before the recursion below,
        so cyclic role graphs remain safe.
     */
    if ((roleId = jsonGetId(config, host->roles, roleName)) < 0) {
        rError("web", "Cannot find role %s", roleName);
        return R_ERR_CANT_FIND;
    }
    rAddName(abilities, roleName, (void*) 1, 0);
    //  Iterate over items in role (can be abilities or other roles)
    for (ITERATE_JSON_ID(config, roleId, child, childId)) {
        item = child->value;
        if (!item) {
            continue;
        }
        //  Check if item is another role (recursive inheritance)
        if (jsonGetId(config, host->roles, item) >= 0) {
            //  Recursively expand the inherited role
            if (expandRole(host, item, abilities) < 0) {
                return R_ERR_CANT_FIND;
            }
        } else {
            //  It's an ability - add it
            rAddName(abilities, item, (void*) 1, 0);
        }
    }
    return 0;
}

/************************* Authentication Failure Limit ***********************/
/*
    Failures are counted per client address over host->failurePeriod from the first failure. Nothing
    sleeps: a refused attempt is answered at once and holds no fiber.
 */
typedef struct WebAuthFailures {
    Ticks start;                /* When the first failure in the period was recorded */
    int count;                  /* Failures in the period */
} WebAuthFailures;

static bool getClientAddress(Web *web, char *ip, size_t ipLen)
{
    int port;

    return rGetSocketPeer(web->sock, ip, ipLen, &port) == 0 && *ip;
}

/*
    Return the live failure record for an address. An expired record is removed.
 */
static WebAuthFailures *lookupFailures(WebHost *host, cchar *ip)
{
    WebAuthFailures *fp;

    if ((fp = rLookupName(host->authFailures, ip)) != 0 && rGetTicks() - fp->start >= host->failurePeriod) {
        rRemoveName(host->authFailures, ip);
        fp = 0;
    }
    return fp;
}

static void pruneFailures(WebHost *host)
{
    WebAuthFailures *fp;
    RName           *np;
    RList           *expired;
    Ticks           now;
    cchar           *ip;
    int             next;

    expired = rAllocList(0, 0);
    now = rGetTicks();
    for (ITERATE_NAMES(host->authFailures, np)) {
        fp = np->value;
        if (now - fp->start >= host->failurePeriod) {
            rAddItem(expired, sclone(np->name));
        }
    }
    for (ITERATE_ITEMS(expired, ip, next)) {
        rRemoveName(host->authFailures, ip);
        rFree((char*) ip);
    }
    rFreeList(expired);
}

PUBLIC bool webAuthBlocked(Web *web)
{
    WebAuthFailures *fp;
    char            ip[64];

    if (!web || web->host->maxFailures <= 0 || !getClientAddress(web, ip, sizeof(ip))) {
        return 0;
    }
    fp = lookupFailures(web->host, ip);
    return fp && fp->count >= web->host->maxFailures;
}

PUBLIC void webAuthFailed(Web *web)
{
    WebHost         *host;
    WebAuthFailures *fp;
    char            ip[64];

    if (!web || web->host->maxFailures <= 0 || !getClientAddress(web, ip, sizeof(ip))) {
        return;
    }
    host = web->host;
    if ((fp = lookupFailures(host, ip)) == 0) {
        if (rGetHashLength(host->authFailures) >= ME_WEB_MAX_AUTH_CLIENTS) {
            pruneFailures(host);
            if (rGetHashLength(host->authFailures) >= ME_WEB_MAX_AUTH_CLIENTS) {
                rTrace("auth", "Too many clients with authentication failures to track");
                return;
            }
        }
        fp = rAllocType(WebAuthFailures);
        fp->start = rGetTicks();
        rAddName(host->authFailures, ip, fp, 0);
    }
    fp->count++;
}

/******************* HTTP Authentication (Basic & Digest) ********************/
#if ME_WEB_HTTP_AUTH
/*
    Treat users without stored passwords as unknown; an empty Digest HA1 is publicly computable.
 */
static WebUser *lookupHttpUser(Web *web)
{
    WebUser *user;

    user = webLookupUser(web->host, web->username);
    return webUserHasCredential(user) ? user : 0;
}

/*
    HTTP authentication coordinator
    Handles Basic and Digest authentication from Authorization header
    Returns true if authenticated and authorized for the route
 */
PUBLIC bool webHttpAuthenticate(Web *web)
{
    WebUser  *user;
    WebRoute *route;
    cchar    *algorithm, *requiredAuthType, *uri;

    if (!web) {
        return 0;
    }
    route = web->route;

    //  No Authorization header - send challenge
    if (!web->authType || !web->authDetails) {
        sendAuthChallenge(web, route);
        return 0;
    }
    if (webAuthBlocked(web)) {
        webError(web, 429, "Too many authentication failures");
        return 0;
    }
    //  Determine required auth type (route overrides host default)
    requiredAuthType = route->authType ? route->authType : web->host->authType;

    //  Parse and verify credentials based on auth type
#if ME_WEB_AUTH_BASIC
    if (scaselessmatch(web->authType, "Basic")) {
        /*
            If route requires digest, reject Basic auth and send Digest challenge
            This must be checked BEFORE TLS enforcement to allow client auto-upgrade
         */
        if (requiredAuthType && scaselessmatch(requiredAuthType, "digest")) {
            sendAuthChallenge(web, route);
            return 0;
        }
        // Enforce TLS for Basic if configured (only if Basic is actually acceptable)
        if (web->host->requireTlsForBasic && !rIsSocketSecure(web->sock)) {
            webError(web, 403, "Basic authentication requires HTTPS");
            return 0;
        }
        if (!parseBasicAuth(web)) {
            sendAuthChallenge(web, route);
            return 0;
        }
        /*
            Use dummy credentials for unknown users to reduce timing differences.
         */
        user = lookupHttpUser(web);
        if (!verifyPassword(web, user ? user : &web->host->dummyUser) || !user) {
            webAuthFailed(web);
            sendAuthChallenge(web, route);
            return 0;
        }
        //  Success - set authentication state
        web->user = user;
        rFree(web->role);
        web->role = sclone(user->role);
        web->authenticated = 1;
        return 1;
    }
#endif

#if ME_WEB_AUTH_DIGEST
    if (scaselessmatch(web->authType, "Digest")) {

        if (requiredAuthType && scaselessmatch(requiredAuthType, "basic")) {
            sendAuthChallenge(web, route);
            return 0;
        }
        if (!parseDigestAuth(web)) {
            sendAuthChallenge(web, route);
            return 0;
        }
        // Determine server algorithm (server is authoritative per RFC 7616)
        algorithm = (route && route->algorithm) ? route->algorithm :
                    (web->host->algorithm ? web->host->algorithm : "SHA-256");

        /*
            If client didn't specify algorithm, assume they're using server's algorithm
            RFC 7616: Server sends algorithm in WWW-Authenticate challenge, client echoes it back
            Client CANNOT override server's algorithm choice (prevents downgrade attacks)
         */
        if (!web->algorithm) {
            web->algorithm = sclone(algorithm);
        }
        // Enforce algorithm matches server-selected algorithm (reject mismatches)
        if (!scaselessmatch(web->algorithm, algorithm)) {
            sendAuthChallenge(web, route);
            return 0;
        }
        /*
            Validate the nonce and check the nonce count, without recording anything yet. The
            replay state is committed only once the digest has verified, below. Every failure from
            here to that point leaves the tracking entry exactly as it was: an unauthenticated
            request must not be able to advance, create or delete replay state, or a captured
            request could be replayed by first sending a bad one to reset the nonce.
         */
        if (!validateNonce(web)) {
            sendAuthChallenge(web, route);
            return 0;
        }
        /*
            Enforce URI binding to the actual request-target
         */
        uri = web->uri;
        if (!uri || !web->path) {
            sendAuthChallenge(web, route);
            return 0;
        }
        if (!matchDigestUri(web, uri)) {
            sendAuthChallenge(web, route);
            return 0;
        }
        /*
            Use a dummy password for unknown users to reduce timing differences.
         */
        user = lookupHttpUser(web);
        web->digest = computeDigest(web, user ? user->password : web->host->dummyDigestPassword);
        if (!web->digest || !cryptMatch(web->digest, web->digestResponse) || !user) {
            webAuthFailed(web);
            sendAuthChallenge(web, route);
            return 0;
        }
        //  The request is authentic - record its nonce count so it cannot be replayed
        if (!commitNonce(web)) {
            sendAuthChallenge(web, route);
            return 0;
        }
        //  Check authorization
        if (!webUserCan(user, route->role)) {
            webError(web, 403, "Access Denied. Insufficient privilege.");
            return 0;
        }
        //  Success - set authentication state
        web->user = user;
        rFree(web->role);
        web->role = sclone(user->role);
        web->authenticated = 1;
        return 1;
    }
#endif
    //  Unknown or unsupported auth type
    sendAuthChallenge(web, route);
    return 0;
}

/*
    Send authentication challenge based on route and host configuration
 */
static void sendAuthChallenge(Web *web, WebRoute *route)
{
    cchar *authType;

    /*
        Determine auth type to use for challenge
        Never challenge on public routes; allow request to continue
     */
    authType = route->authType ? route->authType : web->host->authType;
    if (route && route->role && smatch(route->role, "public")) {
        // Do not alter response; caller should handle as anonymous
        return;
    }
    if (authType && scaselessmatch(authType, "digest")) {
#if ME_WEB_AUTH_DIGEST
        sendDigestChallenge(web, route);
#else
        // Never downgrade Digest to Basic, which sends reusable credentials.
        webError(web, 500, "Digest authentication is not supported in this build");
#endif
        return;
    }
#if ME_WEB_AUTH_BASIC
    /* Never offer Basic over cleartext; refusing the credential after receipt cannot protect it. */
    if (web->host->requireTlsForBasic && !rIsSocketSecure(web->sock)) {
        webError(web, 403, "Authentication requires HTTPS");
        return;
    }
    sendBasicChallenge(web);
#else
    webError(web, 401, "Authentication required but not configured");
#endif
}

/******************************** Helper Functions ****************************/
/*
    Compute hash of string using specified algorithm
    Returns hex-encoded hash string
    Supports: MD5, SHA-256
 */
PUBLIC char *webHash(cchar *str, cchar *algorithm)
{
    if (!str) {
        return 0;
    }
    if (!algorithm || scaselessmatch(algorithm, "SHA-256")) {
        // SHA-256 is the default (recommended)
        return cryptGetSha256((cuchar*) str, slen(str));

    } else if (scaselessmatch(algorithm, "MD5")) {
#if ME_DEBUG
        // MD5 for legacy digest compatibility
        static bool md5Warned = 0;
        if (!md5Warned) {
            rTrace("web", "MD5 algorithm is deprecated and cryptographically weak - migrate to SHA-256");
            md5Warned = 1;
        }
#endif
        return cryptGetMd5((uchar*) str, slen(str));
    }
    // Default to SHA-256
    return cryptGetSha256((cuchar*) str, slen(str));
}

/*
    Free a hash preimage.

    Every preimage below is built with sfmt rather than SFMT into a fixed buffer. SFMT truncates
    silently, and these preimages are assembled from client-supplied username, uri, nonce, nc and
    cnonce: two inputs differing only past the truncation point produce the same hash, which in a
    credential comparator is a collision the attacker chooses. An allocated buffer cannot truncate,
    and this is at most one allocation per authentication - nothing next to the SHA-256 that follows.

    The buffer holds the plaintext password on the stored-password paths and HA1 on the digest
    paths, so clear it rather than leaving it in freed heap. Written through a volatile pointer so
    the compiler cannot drop the stores as dead.
 */
static void freePreimage(char *buf)
{
    volatile char *p;
    size_t        i, len;

    if (!buf) {
        return;
    }
    len = slen(buf);
    p = (volatile char*) buf;
    for (i = 0; i < len; i++) {
        p[i] = '\0';
    }
    rFree(buf);
}

/*
    Hash password for storage
    Format: H(username:realm:password) where H is the algorithm
    Default is SHA-256
 */
PUBLIC char *webHashPassword(WebHost *host, cchar *username, cchar *password)
{
    char  *buf, *result;
    cchar *realm, *algorithm;

    if (!host || !username || !password) {
        return 0;
    }
    realm = host->realm ? host->realm : host->name;
    algorithm = host->algorithm ? host->algorithm : "SHA-256";
    buf = sfmt("%s:%s:%s", username, realm, password);
    result = webHash(buf, algorithm);
    freePreimage(buf);
    return result;
}

/*
    Compare a plain-text password against a stored password hash.

    This is the single definition of the stored password format. A stored hash may carry an
    algorithm prefix - "BF1:", "MD5:" or "SHA256:" - which selects how it is verified; without a
    prefix the host's configured algorithm is used, which is the legacy form. HTTP Basic,
    webVerifyUserPassword and application logins all go through here, so they cannot drift apart.

    A null stored value is an unknown user: verify against the decoy so the cost matches a known user.
    Comparison is constant time via cryptMatch to avoid leaking the hash through timing.
 */
PUBLIC bool webCheckPassword(WebHost *host, cchar *username, cchar *password, cchar *stored)
{
    char  *buf, *encoded;
    cchar *algorithm, *realm;
    bool  match, known;

    if (!host || !username || !password) {
        return 0;
    }
    known = stored != 0;
    if (!known && (stored = host->decoyPassword) == 0) {
        return 0;
    }
    realm = host->realm ? host->realm : host->name;
    buf = sfmt("%s:%s:%s", username, realm, password);

    if (sstarts(stored, "BF1:")) {
        /*
            Bcrypt carries its salt in the stored value, so cryptCheckPassword re-encrypts with
            that salt rather than comparing a precomputed hash.
         */
        match = cryptCheckPassword(buf, stored);
        freePreimage(buf);
        return match && known;
    }
    if (sstarts(stored, "MD5:")) {
        algorithm = "MD5";
        stored += 4;
    } else if (sstarts(stored, "SHA256:")) {
        algorithm = "SHA-256";
        stored += 7;
    } else if (sstarts(stored, "SHA512:")) {
        //  Recognised in stored data but not supported
        freePreimage(buf);
        return 0;
    } else {
        algorithm = host->algorithm ? host->algorithm : "SHA-256";
    }
    encoded = webHash(buf, algorithm);
    freePreimage(buf);
    match = cryptMatch(encoded, stored);
    rFree(encoded);
    return match;
}

/*
    Verify plain-text password against stored hash
 */
PUBLIC bool webVerifyUserPassword(WebHost *host, cchar *username, cchar *password)
{
    WebUser *user;

    if (!host || !username || !password) {
        return 0;
    }
    user = webLookupUser(host, username);
    return webCheckPassword(host, username, password, user ? user->password : 0);
}

/*
    Decode Base64 string
 */
PUBLIC char *webDecode64(cchar *str)
{
    size_t len;
    char   *decoded;

    if (!str) {
        return 0;
    }
    decoded = cryptDecode64Block(str, &len, 0);
    if (!decoded) {
        return 0;
    }
    //  Already null-terminated by cryptDecode64Block
    return decoded;
}

/*
    Encode string as Base64
 */
PUBLIC char *webEncode64(cchar *str)
{
    if (!str) {
        return 0;
    }
    return cryptEncode64Block((cuchar*) str, slen(str));
}

/****************************** Basic Authentication **************************/
#if ME_WEB_AUTH_BASIC
/*
    Parse Basic authentication header
    Format: "Authorization: Basic <base64(username:password)>"
 */
static bool parseBasicAuth(Web *web)
{
    char *decoded, *cp;

    if (!web->authDetails) {
        return 0;
    }
    if ((decoded = webDecode64(web->authDetails)) == 0) {
        return 0;
    }
    rFree(web->username);
    web->username = 0;
    rFree(web->password);
    web->password = 0;

    //  Split username:password
    if ((cp = strchr(decoded, ':')) != 0) {
        *cp++ = '\0';
        /*
            Bound both halves at the same limit webAddUser applies to a stored credential, so the
            two ends agree. Without this the only bound is web.limits.header, and an over-long
            credential is hashed in a degraded form rather than refused.
         */
        if (slen(decoded) > ME_WEB_MAX_AUTH || slen(cp) > ME_WEB_MAX_AUTH) {
            rFree(decoded);
            return 0;
        }
        web->username = sclone(decoded);
        web->password = sclone(cp);
        web->encoded = 0;
    }
    rFree(decoded);
    return web->username && *web->username;
}

/*
    Send 401 Unauthorized with Basic challenge
 */
static void sendBasicChallenge(Web *web)
{
    cchar *realm;

    realm = web->host->realm ? web->host->realm : web->host->name;
    webSetStatus(web, 401);
    webAddHeader(web, "WWW-Authenticate", "Basic realm=\"%s\", charset=\"UTF-8\"", realm);
    webFinalize(web);
}

/*
    Verify password for Basic or Digest auth
    Uses constant-time comparison from crypt module
    Supports multiple hash algorithms (MD5, SHA-256)
 */
static bool verifyPassword(Web *web, WebUser *user)
{
    if (!web->password || !user) {
        return 0;
    }
    if (web->encoded) {
        /*
            The client supplied an already-hashed password, so there is nothing left to hash.
            Compare it against the stored value directly, in constant time.
         */
        return cryptMatch(web->password, user->password);
    }
    return webCheckPassword(web->host, web->username, web->password, user->password);
}
#endif /* ME_WEB_AUTH_BASIC */

/****************************** Digest Authentication *************************/
#if ME_WEB_AUTH_DIGEST
/*
    Create a nonce for digest authentication.
    Format (base64): ts:rnd:mac (32-byte binary HMAC)
    mac = HMAC-SHA256(secret, realm:algorithm:ts:rnd)
 */
static char *createNonce(Web *web, cchar *algorithm)
{
    char   *macInput, *tsRndBuf;
    uchar  mac[CRYPT_HMAC_SHA256_SIZE];
    char   *rnd, *result;
    RBuf   *payload;
    cchar  *secret, *realm;
    Ticks  now;
    size_t tsRndLen;

    secret = web->host->secret;
    realm = web->host->realm ? web->host->realm : web->host->name;
    now = rGetTime();
    if ((rnd = cryptID(32)) == 0) {
        return 0;
    }
    macInput = sfmt("%s:%s:%llx:%s", realm, algorithm ? algorithm : "SHA-256", (long long) now, rnd);
    cryptGetHmacSha256Block((cuchar*) secret, slen(secret), (cuchar*) macInput, slen(macInput), mac);
    freePreimage(macInput);

    // Build payload: timestamp:random:binaryMAC
    tsRndBuf = sfmt("%llx:%s:", (long long) now, rnd);
    tsRndLen = slen(tsRndBuf);
    payload = rAllocBuf(tsRndLen + CRYPT_HMAC_SHA256_SIZE);
    rPutBlockToBuf(payload, tsRndBuf, tsRndLen);
    rPutBlockToBuf(payload, (char*) mac, CRYPT_HMAC_SHA256_SIZE);

    result = cryptEncode64Block((cuchar*) rGetBufStart(payload), rGetBufLength(payload));
    freePreimage(tsRndBuf);
    rFree(rnd);
    rFreeBuf(payload);
    return result;
}

/*
    Validate nonce hasn't expired and verify HMAC
 */
static bool validateNonce(Web *web)
{
    WebNonceEntry *entry;
    Ticks         when, now;
    size_t        decodedLen, textLen;
    uchar         expectedMac[CRYPT_HMAC_SHA256_SIZE], *decoded, *receivedMac;
    cchar         *realm, *secret, *algorithm;
    char          *whenStr, *rnd, *tok, *macInput;
    uint64        currentNc;
    int           age, rc;
    bool          match;

    if (!web->nonce) {
        return 0;
    }
    /*
        Decode base64 nonce to get binary payload
        The payload is: "timestamp:random:" (text) + binaryMAC (32 bytes)
     */
    decoded = (uchar*) cryptDecode64Block(web->nonce, &decodedLen, 0);
    if (!decoded || decodedLen < CRYPT_HMAC_SHA256_SIZE + 10) {
        // Need at least "ts:rnd:" plus 32-byte MAC
        rFree(decoded);
        return 0;
    }
    textLen = decodedLen - CRYPT_HMAC_SHA256_SIZE;
    receivedMac = decoded + textLen;

    // Replace trailing ':' with null terminator for stok (MAC remains untouched after textLen)
    decoded[textLen - 1] = '\0';

    // Parse timestamp and random directly from decoded (saves an allocation)
    whenStr = stok((char*) decoded, ":", &tok);
    rnd = stok(NULL, ":", &tok);
    rc = 0;
    if (whenStr && rnd) {
        // Validate timestamp
        when = (Ticks) stoix(whenStr, NULL, 16);
        now = rGetTime();
        //  The timestamp is client supplied: only a past time has a meaningful age
        age = (when > 0 && when <= now) ? (int) ((now - when) / TPS) : -1;
        if (age >= 0 && age <= web->host->digestTimeout) {
            // Compute expected HMAC using same inputs
            realm = web->host->realm ? web->host->realm : web->host->name;
            secret = web->host->secret;
            algorithm = (web->route && web->route->algorithm) ? web->route->algorithm :
                        (web->host->algorithm ? web->host->algorithm : "SHA-256");
            macInput = sfmt("%s:%s:%s:%s", realm, algorithm, whenStr, rnd);
            cryptGetHmacSha256Block((cuchar*) secret, slen(secret), (cuchar*) macInput, slen(macInput), expectedMac);
            freePreimage(macInput);
            match = cryptMatchHmacSha256(expectedMac, receivedMac);
            if (match) {
                /*
                    Replay protection: the nonce count must increase on every use of a nonce.
                    This only checks; nothing is recorded until commitNonce, after the digest
                    has verified. Tracking is skipped when trackNonces is off (benchmarks).
                 */
                if (web->host->trackNonces) {
                    if (parseNc(web->nc, &currentNc)) {
                        entry = (WebNonceEntry*) rLookupName(web->host->nonces, web->nonce);
                        if (entry) {
                            rc = currentNc > entry->lastNc;
                        } else {
                            /*
                                A nonce with no entry has never been used successfully, so this
                                must be its first request. Requiring nc == 1 means that even if an
                                entry were somehow lost, the replay of a later request could not be
                                accepted as a fresh first use.
                             */
                            if (rGetHashLength(web->host->nonces) > web->host->maxDigest) {
                                pruneNonces(web->host);
                            }
                            if (rGetHashLength(web->host->nonces) > web->host->maxDigest) {
                                static bool warned = false;
                                if (!warned) {
                                    rError("web", "Digest authentication nonce limit reached: %d",
                                           web->host->maxDigest);
                                    warned = true;
                                }
                            } else if (nonceUserCount(web->host, web->username) >=
                                       max(1, web->host->maxDigest / WEB_DIGEST_USERS)) {
                                /* Limit each user's nonce share so one account cannot exhaust Digest capacity. */
                                rDebug("web", "Digest nonce share reached for user %s", web->username);
                            } else {
                                rc = currentNc == 1;
                            }
                        }
                    }
                } else {
                    //  Tracking disabled (benchmarks only)
                    rc = 1;
                }
            }
        }
    }
    rFree(decoded);
    return rc;
}

/*
    Parse a nonce count. RFC 7616 defines nc as exactly eight hexadecimal digits, so anything
    else is refused rather than passed to a lenient converter. The value is returned unsigned and
    untruncated: counts of 0x80000000 and above are legitimate and must compare correctly.
 */
static bool parseNc(cchar *nc, uint64 *value)
{
    uint64 result;
    int    i, c;

    if (!nc || slen(nc) != 8) {
        return 0;
    }
    result = 0;
    for (i = 0; i < 8; i++) {
        c = tolower((uchar) nc[i]);
        if (c >= '0' && c <= '9') {
            result = (result << 4) | (uint64) (c - '0');
        } else if (c >= 'a' && c <= 'f') {
            result = (result << 4) | (uint64) (10 + c - 'a');
        } else {
            return 0;
        }
    }
    *value = result;
    return 1;
}

/*
    Update replay state only after Digest verification. Failed requests neither change counts nor remove entries;
    pruneNonces reclaims expired entries.
 */
static bool commitNonce(Web *web)
{
    WebNonceEntry *entry;
    uint64        nc;

    if (!web->host->trackNonces) {
        return 1;
    }
    if (!parseNc(web->nc, &nc)) {
        return 0;
    }
    entry = (WebNonceEntry*) rLookupName(web->host->nonces, web->nonce);
    if (entry) {
        entry->lastNc = nc;
        return 1;
    }
    /*
        validateNonce already refused the request if the table was full, and nothing yields
        between the check and here, so the entry can be created. It is dated from now rather than
        from the nonce's own timestamp: the nonce still expires on its issue time in validateNonce,
        so an entry that outlives it by the difference is harmless and is reclaimed on the next
        cleanup.
     */
    //  The username is stored in the same allocation, so the table's R_DYNAMIC_VALUE free releases both
    entry = rAlloc(sizeof(WebNonceEntry) + slen(web->username) + 1);
    memset(entry, 0, sizeof(WebNonceEntry));
    entry->created = rGetTime();
    entry->lastNc = nc;
    entry->username = (char*) &entry[1];
    scopy(entry->username, slen(web->username) + 1, web->username);
    rAddName(web->host->nonces, web->nonce, entry, 0);
    addNonceUser(web->host, entry->username, 1);
    return 1;
}

static Ticks getNonceCleanupPeriod(WebHost *host)
{
    return (Ticks) max(1, min(30, host->digestTimeout / 2)) * TPS;
}


static void pruneNonces(WebHost *host)
{
    WebNonceEntry *entry;
    RName         *np;
    RList         *toDelete;
    Ticks         now, cutoff;
    char          *nonceName;
    int           next;

    if (rGetHashLength(host->nonces) > 0) {
        now = rGetTime();
        cutoff = now - (host->digestTimeout * TPS);
        toDelete = rAllocList(0, R_TEMPORAL_VALUE);

        // Collect expired nonces (can't modify hash while iterating)
        for (ITERATE_NAMES(host->nonces, np)) {
            entry = (WebNonceEntry*) np->value;
            if (entry && entry->created < cutoff) {
                rAddItem(toDelete, np->name);
            }
        }
        // Remove expired nonces
        for (ITERATE_ITEMS(toDelete, nonceName, next)) {
            entry = (WebNonceEntry*) rLookupName(host->nonces, nonceName);
            if (entry) {
                addNonceUser(host, entry->username, -1);
                rRemoveName(host->nonces, nonceName);
            }
        }
        rFreeList(toDelete);
    }
}


static ssize nonceUserCount(WebHost *host, cchar *username)
{
    return (ssize) rLookupName(host->nonceUsers, username);
}

static void addNonceUser(WebHost *host, cchar *username, ssize delta)
{
    ssize count;

    if (!username) {
        return;
    }
    count = nonceUserCount(host, username) + delta;
    if (count > 0) {
        rAddName(host->nonceUsers, username, (void*) count, 0);
    } else {
        rRemoveName(host->nonceUsers, username);
    }
}

/*
    Only the timer re-arms; synchronous pruning must not start another timer chain.
 */
static void cleanupNonces(void *arg)
{
    WebHost *host;

    host = (WebHost*) arg;
    pruneNonces(host);
    host->nonceCleanupEvent = rAllocEvent(NULL, (REventProc) cleanupNonces, host,
                                          getNonceCleanupPeriod(host), 0);
}

/*
    Initialize digest authentication (start nonce cleanup timer)
 */
PUBLIC void webInitDigestAuth(WebHost *host)
{
    host->nonceCleanupEvent = rAllocEvent(NULL, (REventProc) cleanupNonces, host,
                                          getNonceCleanupPeriod(host), 0);
}

/*
    Send 401 Unauthorized with Digest challenge
 */
static void sendDigestChallenge(Web *web, WebRoute *route)
{
    cchar *realm, *algorithm, *opaque;
    char  *nonce;

    // Use route algorithm if specified, otherwise fall back to host algorithm
    algorithm = (route && route->algorithm) ? route->algorithm :
                (web->host->algorithm ? web->host->algorithm : "SHA-256");
    realm = web->host->realm ? web->host->realm : web->host->name;
    opaque = web->host->opaque ? web->host->opaque : "opaque";

    if ((nonce = createNonce(web, algorithm)) == 0) {
        webError(web, 500, "Cannot generate authentication nonce");
        return;
    }

    webSetStatus(web, 401);
    webAddHeader(web, "WWW-Authenticate",
                 "Digest realm=\"%s\", qop=\"auth\", nonce=\"%s\", opaque=\"%s\", algorithm=\"%s\"",
                 realm, nonce, opaque, algorithm);
    rFree(nonce);
    webFinalize(web);
}

/*
    One bit per Digest auth-param, to detect a repeat.
 */
#define WEB_DIGEST_USERNAME  0x001
#define WEB_DIGEST_REALM     0x002
#define WEB_DIGEST_NONCE     0x004
#define WEB_DIGEST_URI       0x008
#define WEB_DIGEST_QOP       0x010
#define WEB_DIGEST_NC        0x020
#define WEB_DIGEST_ALGORITHM 0x040
#define WEB_DIGEST_CNONCE    0x080
#define WEB_DIGEST_RESPONSE  0x100
#define WEB_DIGEST_OPAQUE    0x200

/*
    Parse Digest authentication header
    Format: Digest username="...", realm="...", nonce="...", uri="...", response="...", ...

    RFC 7616 section 3.4 defines the credentials as a set of auth-params, so a repeated parameter
    is malformed and is rejected. Resolving a duplicate by taking the last occurrence would let one
    header carry two readings of the same field, so a front-end that reads the first and a server
    that reads the last disagree about who is asking.
 */
static bool parseDigestAuth(Web *web)
{
    char  *key, *value, *tok, *details, *opaque, **field, *dst;
    cchar *realm;
    int   bit, duplicate, malformed, seen;

    if (!web->authDetails) {
        return 0;
    }
    details = sclone(web->authDetails);
    key = details;
    duplicate = 0;
    malformed = 0;
    seen = 0;

    while (*key) {
        //  Skip whitespace
        while (*key && isspace((uchar) * key)) key++;
        //  Find key
        for (tok = key; *tok && *tok != '=' && *tok != ','; tok++) {
        }
        if (*tok != '=') {
            if (tok == key && *tok == ',') {

                key = tok + 1;
                continue;
            }
            //  A token without a value fails the parse rather than silently ending it
            malformed = tok != key;
            break;
        }
        *tok++ = '\0';
        /*
            Unescape quoted pairs in place; output cannot exceed input length.
         */
        if (*tok == '"') {
            value = dst = ++tok;
            while (*tok && *tok != '"') {
                if (*tok == '\\' && tok[1]) {
                    tok++;
                }
                *dst++ = *tok++;
            }
            if (*tok == '"') {
                tok++;
            }
            *dst = '\0';
        } else {
            value = tok;
            while (*tok && *tok != ',') tok++;
        }
        if (*tok == ',') *tok++ = '\0';

        //  Map the parameter to the field that holds it and to its seen-bit
        field = 0;
        bit = 0;
        if (scaselessmatch(key, "username")) {
            field = &web->username;
            bit = WEB_DIGEST_USERNAME;
        } else if (scaselessmatch(key, "realm")) {
            field = &web->realm;
            bit = WEB_DIGEST_REALM;
        } else if (scaselessmatch(key, "nonce")) {
            field = &web->nonce;
            bit = WEB_DIGEST_NONCE;
        } else if (scaselessmatch(key, "uri")) {
            field = &web->uri;
            bit = WEB_DIGEST_URI;
        } else if (scaselessmatch(key, "qop")) {
            field = &web->qop;
            bit = WEB_DIGEST_QOP;
        } else if (scaselessmatch(key, "nc")) {
            field = &web->nc;
            bit = WEB_DIGEST_NC;
        } else if (scaselessmatch(key, "algorithm")) {
            field = &web->algorithm;
            bit = WEB_DIGEST_ALGORITHM;
        } else if (scaselessmatch(key, "cnonce")) {
            field = &web->cnonce;
            bit = WEB_DIGEST_CNONCE;
        } else if (scaselessmatch(key, "response")) {
            field = &web->digestResponse;
            bit = WEB_DIGEST_RESPONSE;
        } else if (scaselessmatch(key, "opaque")) {
            field = &web->opaque;
            bit = WEB_DIGEST_OPAQUE;
        }
        if (field) {
            if (seen & bit) {
                duplicate = 1;
                break;
            }
            seen |= bit;
            rFree(*field);
            *field = sclone(value);
        }
        key = tok;
    }
    rFree(details);

    if (duplicate || malformed) {
        return 0;
    }
    /* Require qop=auth, nc and cnonce as offered; the no-qop form lacks replay protection. */
    if (!web->username || !web->digestResponse || !web->realm || !web->nonce || !web->uri) {
        return 0;
    }
    if (!web->qop || !scaselessmatch(web->qop, "auth") || !web->nc || !web->cnonce) {
        return 0;
    }
    /*
        Validate field lengths. nc is 8 hex digits per RFC 7616 and cnonce is a client-chosen
        token; neither was bounded, so both reached the digest preimage at whatever length the
        header allowed.
     */
    if (slen(web->username) > 64 || slen(web->nonce) > 256 || slen(web->uri) > 2048 || slen(web->realm) > 128) {
        return 0;
    }
    if (slen(web->nc) > 16 || slen(web->cnonce) > 128) {
        return 0;
    }
    //  Validate algorithm is in whitelist
    if (web->algorithm) {
        if (!smatch(web->algorithm, "MD5") && !smatch(web->algorithm, "SHA-256")) {
            return 0;
        }
    }
    //  Validate opaque value matches what server sent (RFC 7616 compliance)
    if (web->opaque) {
        opaque = web->host->opaque ? web->host->opaque : "opaque";
        if (!smatch(web->opaque, opaque)) {
            return 0;
        }
    }
    realm = web->host->realm ? web->host->realm : web->host->name;
    if (!smatch(web->realm, realm)) {
        return 0;
    }
    return 1;
}

/*
    Compare the digest uri parameter against the request target.
    RFC 7616 section 3.4 makes the digest uri the Request-URI, so it carries the query string
    whenever the request has one. The uri may be sent relative, without the leading "/".
 */
static bool matchDigestUri(Web *web, cchar *uri)
{
    cchar  *path, *query;
    size_t len;

    if ((query = schr(uri, '?')) != 0) {
        len = (size_t) (query - uri);
        query++;
    } else {
        len = slen(uri);
    }
    path = web->path;
    if (*uri != '/' && *path == '/') {
        path++;
    }
    if (slen(path) != len || sncmp(uri, path, len) != 0) {
        return 0;
    }
    return smatch(query, web->query);
}

/*
    Compute digest response per RFC 7616
    response = H(HA1:nonce:nc:cnonce:qop:HA2)
    where HA1 = H(username:realm:password), HA2 = H(method:uri)
 */
static char *computeDigest(Web *web, cchar *password)
{
    char  *a2Buf, *digestBuf;
    char  *ha1, *ha2, *result;
    cchar *algorithm, *hashValue, *passwordAlg;

    if (!password || !web->nonce || !web->uri) {
        return 0;
    }
    // Enforce server-selected algorithm (route overrides host)
    algorithm = (web->route && web->route->algorithm) ? web->route->algorithm :
                (web->host->algorithm ? web->host->algorithm : "SHA-256");

    /*
        HA1 = H(username:realm:password) - password is already hashed
        Strip algorithm prefix (MD5:, SHA256:, SHA512:, BF1:) if present
        and validate that password algorithm matches digest algorithm
     */
    passwordAlg = NULL;
    if (sstarts(password, "MD5:")) {
        passwordAlg = "MD5";
        hashValue = password + 4;
    } else if (sstarts(password, "SHA256:")) {
        passwordAlg = "SHA-256";
        hashValue = password + 7;
    } else if (sstarts(password, "BF1:")) {
        passwordAlg = "BF1";
        hashValue = password + 4;
    } else {
        hashValue = password;
    }
    // Validate algorithm match
    if (passwordAlg) {
        if (scaselessmatch(passwordAlg, "BF1")) {
            rDebug("web", "User '%s' has bcrypt password - cannot use with Digest authentication for URI %s",
                   web->username, web->uri);
            return 0;
        } else if (!scaselessmatch(passwordAlg, algorithm)) {
            rDebug("web", "User '%s' password algorithm (%s) does not match digest algorithm (%s) for URI %s",
                   web->username, passwordAlg, algorithm, web->uri);
            return 0;
        }
    }
    ha1 = sclone(hashValue);

    //  HA2 = H(method:uri)
    a2Buf = sfmt("%s:%s", web->method, web->uri);
    ha2 = webHash(a2Buf, algorithm);
    freePreimage(a2Buf);

    //  Final digest. parseDigestAuth has required qop=auth, nc and cnonce.
    digestBuf = sfmt("%s:%s:%s:%s:%s:%s", ha1, web->nonce, web->nc, web->cnonce, web->qop, ha2);
    result = webHash(digestBuf, algorithm);
    freePreimage(digestBuf);

    rFree(ha1);
    rFree(ha2);
    return result;
}
#endif /* ME_WEB_AUTH_DIGEST */

/*
    Build dummy credentials to reduce timing differences for unknown users.
 */
PUBLIC void webInitAuthDummy(WebHost *host)
{
    cchar *realm, *placeholder;

    realm = host->realm ? host->realm : host->name;
    placeholder = "dummy-user-for-timing-equalization";

    /*
        Application logins check unknown users against this. It has the default bcrypt cost, which the
        password utility uses.
     */
    {
        char *buf;

        buf = sfmt("%s:%s:%s", "decoy", realm, placeholder);
        host->decoyPassword = cryptMakePassword(buf, CRYPT_BLOWFISH_SALT_LENGTH, CRYPT_BLOWFISH_ROUNDS);
        freePreimage(buf);
    }

#if ME_WEB_AUTH_BASIC
    /*
        Match a configured bcrypt user's cost.
     */
    {
        RName   *np;
        WebUser *user;
        cchar   *bcryptHash;
        char    *buf, *rounds, *tok, *last;
        int64   roundCount;

        bcryptHash = 0;
        for (ITERATE_NAMES(host->users, np)) {
            user = (WebUser*) np->value;
            if (user->password && sstarts(user->password, "BF1:")) {
                bcryptHash = user->password;
                break;
            }
        }
        buf = sfmt("%s:%s:%s", "dummy", realm, placeholder);
        if (bcryptHash) {
            rounds = sclone(&bcryptHash[4]);
            tok = stok(rounds, ":", &last);
            roundCount = tok ? stoi(tok) : 0;
            rFree(rounds);
            host->dummyUser.password = cryptMakePassword(buf, 0, (size_t) (roundCount > 0 ? roundCount : 0));
        } else {
            host->dummyUser.password = webHash(buf, host->algorithm ? host->algorithm : "SHA-256");
        }
        freePreimage(buf);
    }
#endif

#if ME_WEB_AUTH_DIGEST
    /*
        Digest requires a hash-format password, not bcrypt.
     */
    {
        char *buf;

        buf = sfmt("%s:%s:%s", "dummy-digest", realm, placeholder);
        host->dummyDigestPassword = webHash(buf, host->algorithm ? host->algorithm : "SHA-256");
        freePreimage(buf);
    }
#endif
}

#endif /* ME_WEB_HTTP_AUTH */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
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


#if ME_WIN_LIKE && !defined(O_NONBLOCK)
/*
    Windows open() has no non-blocking flag and a FIFO cannot appear in a document tree, so
    the flag is a no-op there. The regular-file check after open() still applies.
 */
    #define O_NONBLOCK 0
#endif

/************************************ Locals **********************************/

typedef struct stat FileInfo;

/************************************ Forwards *********************************/

static int deleteFile(Web *web, char *path);
static void coalesceRanges(Web *web);
static int fixRanges(Web *web, int64 fileSize);
static int getFile(Web *web, char *path, size_t pathSize);
static cchar *getEncoding(Web *web);
static int sendFile(Web *web, int fd, FileInfo *info, cchar *encoding);
static int pickRanges(Web *web, FileInfo *info, cchar *etag);
static int putFile(Web *web, char *path);
static void redirectToDir(Web *web);
static bool pickFile(Web *web, char *path, size_t pathSize, FileInfo *info, cchar **pEncoding);
static bool deniedPath(Web *web, cchar *path);
#if ME_UNIX_LIKE
static bool isStoredPath(WebHost *host, int fd, cchar *expected);
#endif
static int sendFileContent(Web *web, int fd, FileInfo *info);
static void writeRangeHeader(Web *web, WebRange *range, int64 fileSize);
static void fileEtag(Web *web, FileInfo *info, char *buf, size_t size);

/************************************* Code ***********************************/

/*
    Test the request path against the host's deny patterns, one path segment at a time.

    Every segment is tested, not just the last, so "/.git/objects/ab/cdef" is refused and not
    merely "/.git". This runs in webFileHandler ahead of everything else, so it covers GET, HEAD,
    POST, PUT and DELETE alike, and ahead of pickFile, which would otherwise reach a denied file
    through its ".gz" and ".br" pre-compressed variants.

    A denial is 404, never 403: a 403 confirms the file is there, which for "/.git/config" is most
    of what the attacker wanted. It is traced, because an integrator whose legitimate file is being
    refused needs to find that out quickly.
 */
static bool deniedPath(Web *web, cchar *path)
{
    cchar  *pattern;
    char   segment[ME_MAX_FNAME];
    cchar  *sp, *end;
    size_t len;
    int    next;

    if (!web->host->deny || rGetListLength(web->host->deny) == 0) {
        return 0;
    }
    for (sp = path; sp && *sp; sp = end) {
        while (*sp == '/') {
            sp++;
        }
        if (*sp == '\0') {
            break;
        }
        if ((end = schr(sp, '/')) != 0) {
            len = (size_t) (end - sp);
        } else {
            len = slen(sp);
            end = &sp[len];
        }
        if (len >= sizeof(segment)) {
            return 1;
        }
        scopy(segment, sizeof(segment), sp);
        segment[len] = '\0';

        for (ITERATE_ITEMS(web->host->deny, pattern, next)) {
            if (rMatchFile(segment, pattern)) {
                rTrace("web", "Denied \"%s\": segment \"%s\" matches deny pattern \"%s\"",
                       path, segment, pattern);
                return 1;
            }
        }
    }
    return 0;
}

PUBLIC int webFileHandler(Web *web)
{
    char path[ME_MAX_FNAME];
    int  rc;

    /*
        Normalization blocks lexical traversal. Descriptor checks below separately enforce
        containment and exact spelling after filesystem resolution.
     */
    if (sjoinbuf(path, sizeof(path), webGetDocs(web->host), web->path) < 0) {
        return webError(web, 414, "Request URI too long");
    }
    /*
        After the length check, so an over-long path is still answered 414 rather than being
        reported as a denied name, and before the method dispatch so it covers PUT and DELETE.
     */
    if (deniedPath(web, web->path)) {
        return webError(web, 404, "Cannot locate document");
    }

    if (web->get || web->head || web->post) {
        rc = getFile(web, path, sizeof(path));
    } else if (web->put) {
        rc = putFile(web, path);    // PUT always uses original path
    } else if (web->del) {
        rc = deleteFile(web, path); // DELETE uses original path
    } else {
        rc = webError(web, 405, "Unsupported method");
    }
    return rc;
}

/*
    Require document-root containment and stored filename spelling. Return 404 without exposing canonical names.
 */
static bool validateResolvedPath(Web *web, int fd, cchar *path)
{
    WebHost *host;
    char    actual[ME_MAX_FNAME];
    cchar   *expected;
    size_t  docsLen;

#if ME_WIN_LIKE
    char *cp;
#endif

    host = web->host;

    if (!host->canonicalDocs) {
        //  The document root could not be resolved at startup. Serve nothing.
        return 0;
    }
    if (rGetFdPath(fd, actual, sizeof(actual)) < 0) {
        //  Cannot determine what was opened, so cannot prove it is safe to serve
        return 0;
    }
    docsLen = slen(host->canonicalDocs);
    if (!sstarts(actual, host->canonicalDocs)) {
        return 0;
    }
#if ME_WIN_LIKE
    /*
        Both names above are native, so the containment test compares like with like. The name
        below is compared against the requested path, which came from a URL, so reduce the
        resolved name to the URL form first.
     */
    for (cp = &actual[docsLen]; *cp; cp++) {
        if (*cp == '\\') {
            *cp = '/';
        }
    }
#endif
    if (actual[docsLen] != '/' && actual[docsLen] != '\0') {
        return 0;
    }
    /*
        The requested name is the part of the joined path after the document root. It already
        carries any directory index and pre-compressed extension that pickFile appended.
     */
    expected = &path[slen(webGetDocs(host))];

#if ME_UNIX_LIKE
    return isStoredPath(host, fd, expected);
#else
    return smatch(&actual[docsLen], expected);
#endif
}

#if ME_UNIX_LIKE
/* Use readdir to verify stored spelling when case folding or Unicode normalization can affect lookup. */
static bool isStoredName(int dir, cchar *name, struct stat *info)
{
    struct dirent *dp;
    struct stat   alt;
    DIR           *dirp;
    char          *flipped, *cp;
    bool          folds, found;
    int           fd;

    for (cp = (char*) name; *cp && !(*cp & 0x80); cp++) {
    }
    if (*cp == '\0') {
        flipped = sclone(name);
        for (cp = flipped; *cp && !isalpha((uchar) * cp); cp++) {
        }
        if (*cp) {
            *cp = (char) (isupper((uchar) * cp) ? tolower((uchar) * cp) : toupper((uchar) * cp));
        }
        folds = !*cp || (fstatat(dir, flipped, &alt, AT_SYMLINK_NOFOLLOW) == 0 &&
                         alt.st_ino == info->st_ino && alt.st_dev == info->st_dev);
        rFree(flipped);
        if (!folds) {
            return 1;
        }
    }
    found = 0;
    if ((fd = openat(dir, ".", O_RDONLY | O_DIRECTORY)) < 0) {
        return 0;
    }
    if ((dirp = fdopendir(fd)) == 0) {
        close(fd);
        return 0;
    }
    while ((dp = readdir(dirp)) != 0) {
        if (smatch(dp->d_name, name)) {
            found = 1;
            break;
        }
    }
    closedir(dirp);
    return found;
}

/*
    Walk from the document root without following links, verify stored component names, then bind the open
    descriptor by device/inode. Descriptor paths may contain cached lookup spellings.
 */
static bool isStoredPath(WebHost *host, int fd, cchar *expected)
{
    struct stat info, target;
    char        *copy, *name, *prior, *tok;
    int         dir, next;
    bool        valid;

    if (fstat(fd, &target) < 0 || (dir = open(host->canonicalDocs, O_RDONLY | O_DIRECTORY)) < 0) {
        return 0;
    }
    if (fstat(dir, &info) < 0) {
        close(dir);
        return 0;
    }
    copy = sclone(expected);
    valid = 1;
    prior = 0;
    for (name = stok(copy, "/", &tok); name; name = stok(NULL, "/", &tok)) {
        if (prior) {
            if (!S_ISDIR(info.st_mode) || (next = openat(dir, prior, O_RDONLY | O_DIRECTORY | O_NOFOLLOW)) < 0) {
                valid = 0;
                break;
            }
            close(dir);
            dir = next;
        }
        if (fstatat(dir, name, &info, AT_SYMLINK_NOFOLLOW) < 0 || !isStoredName(dir, name, &info)) {
            valid = 0;
            break;
        }
        prior = name;
    }
    close(dir);
    rFree(copy);
    return valid && info.st_ino == target.st_ino && info.st_dev == target.st_dev;
}
#endif

static int getFile(Web *web, char *path, size_t pathSize)
{
    FileInfo info;
    cchar    *encoding;
    int      fd, rc;

    if (!pickFile(web, path, pathSize, &info, &encoding)) {
        webHook(web, WEB_HOOK_NOT_FOUND);
        if (!web->finalized) {
            return webError(web, 404, "Cannot locate document");
        }
        return 0;
    }
    if (web->finalized) {
        return 0;
    }
    /*
        Authorize substituted index and compressed filenames through their destination routes.
     */
    if (!webAuthorizePath(web, &path[slen(webGetDocs(web->host))])) {
        return 0;
    }
    /*
        O_NONBLOCK matters here. open() on a FIFO with no writer blocks until one appears, and this
        is a plain blocking syscall, not a fiber-aware one - so it stalls the thread, and with it
        every fiber and every connection. A single request for a FIFO in the document root wedged
        the whole server. Opening non-blocking lets the type check below run at all.
     */
    if ((fd = open(path, O_RDONLY | O_BINARY | O_NONBLOCK, 0)) < 0) {
        webError(web, 404, "Cannot open document");
        return R_ERR_CANT_OPEN;
    }
    /*
        Serve only regular files, and take the size from the descriptor rather than the earlier
        path-based stat. fstat closes the window in which the file could be replaced or truncated
        between the stat and the send, which would otherwise emit a Content-Length that does not
        match the body and desynchronise a kept-alive connection.
     */
    if (fstat(fd, &info) < 0 || !S_ISREG(info.st_mode)) {
        close(fd);
        webHook(web, WEB_HOOK_NOT_FOUND);
        return webError(web, 404, "Cannot locate document");
    }
    if (!validateResolvedPath(web, fd, path)) {
        close(fd);
        webHook(web, WEB_HOOK_NOT_FOUND);
        return webError(web, 404, "Cannot locate document");
    }
#if ME_UNIX_LIKE
    //  Clear O_NONBLOCK so the send path reads normally
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) & ~O_NONBLOCK);
#endif
    rc = sendFile(web, fd, &info, encoding);
    close(fd);
    return rc;
}

/* Salt SHA-256 file identity and emit 16 hex digits as ETag, hiding inode metadata. Validators change on restart. */
static void fileEtag(Web *web, FileInfo *info, char *buf, size_t size)
{
    uchar  block[4 * sizeof(uint64) + sizeof(web->host->etagSalt)];
    uchar  digest[CRYPT_SHA256_SIZE];
    uint64 fields[4];
    size_t i;

    fields[0] = (uint64) info->st_dev;
    fields[1] = (uint64) info->st_ino;
    fields[2] = (uint64) info->st_size;
    fields[3] = (uint64) info->st_mtime;
    memcpy(block, fields, sizeof(fields));
    memcpy(&block[sizeof(fields)], web->host->etagSalt, sizeof(web->host->etagSalt));
    cryptGetSha256Block(block, sizeof(block), digest);
    for (i = 0; i < 8 && (i * 2 + 2) < size; i++) {
        snprintf(&buf[i * 2], 3, "%02x", digest[i]);
    }
    buf[i * 2] = '\0';
}

static int sendFile(Web *web, int fd, FileInfo *info, cchar *encoding)
{
    char etag[24];
    int  rc;

    fileEtag(web, info, etag, sizeof(etag));

    /*
        Check conditional request headers (If-None-Match, If-Modified-Since)
        Return 304 Not Modified if content hasn't changed
        Per RFC 7232, this check happens before processing ranges
     */
    rc = 0;
    if (web->route->compressed) {
        //  Every response on a compressed route depends on Accept-Encoding, whichever variant it is
        webAddVary(web, "Accept-Encoding");
    }
    if (encoding && web->ranges && web->ranges->next) {
        //  The encoding headers would describe the multipart envelope, so serve the encoded variant whole
        webFreeRanges(web);
    }
    if (webContentNotModified(web, etag, info->st_mtime)) {
        web->txLen = 0;
        web->status = 304;
        webAddHeaderStaticString(web, "Accept-Ranges", "bytes");
        webAddHeaderDynamicString(web, "Last-Modified", webHttpDate(info->st_mtime));
        webAddHeader(web, "ETag", "\"%s\"", etag);

    } else if ((rc = pickRanges(web, info, etag)) < 0) {
        if (rc == R_ERR_BAD_REQUEST) {
            rc = webError(web, 416, "Requested range not satisfiable");
        } else if (rc == R_ERR_WONT_FIT) {
            rc = webError(web, 500, "File is too large to serve on this platform");
        } else {
            rc = webError(web, 500, "Cannot generate range boundary");
        }

    } else {
        //  Always send Last-Modified and ETag headers
        if (info->st_mtime > 0) {
            webAddHeaderDynamicString(web, "Last-Modified", webHttpDate(info->st_mtime));
        }
        webAddHeader(web, "ETag", "\"%s\"", etag);
        webAddHeaderStaticString(web, "Accept-Ranges", "bytes");

        //  Add compression headers if serving compressed file
        if (encoding) {
            webAddHeaderStaticString(web, "Content-Encoding", encoding);
        }
        if (!web->head) {
            rc = sendFileContent(web, fd, info);
        }
    }
    webFinalize(web);
    // Closes connection on negative return
    return rc;
}

/*
    We can't just serve the index even if we know it exists if the path is a directory and does not end in a slash.
    Must do an external redirect to the directory as required by the spec.
    Must preserve query and ref.
 */
static void redirectToDir(Web *web)
{
    RBuf *buf;
    char *url;

    buf = rAllocBuf(0);
    //  A trimmed path is relative to the route prefix the client requested, so put it back
    if (web->trimmed) {
        rPutStringToBuf(buf, web->route->trim);
    }
    rPutStringToBuf(buf, web->path);
    rPutCharToBuf(buf, '/');
    if (web->query) {
        rPutToBuf(buf, "?%s", web->query);
    }
    if (web->hash) {
        rPutToBuf(buf, "#%s", web->hash);
    }
    url = rBufToStringAndFree(buf);
    //  Same-origin by construction: a relative Location cannot name the backend's scheme or port
    webRedirectLocal(web, 301, url);
    rFree(url);
}

/*
    Mutations require descriptor-relative operations. Holding the validated parent prevents a
    replaced symlink from redirecting a later create, rename or unlink. Platforms without these
    primitives fail closed rather than falling back to path-based writes. ME_WEB_MUTATIONS in web.h
    is the single definition of that condition, so callers can ask before they depend on it.
 */
#if ME_WEB_MUTATIONS

static int openFileParent(Web *web, cchar *path)
{
    char *parent;
    int  fd;

    parent = rDirname(sclone(path));
    fd = open(parent, O_RDONLY | O_DIRECTORY | O_NONBLOCK);
    if (fd >= 0 && !validateResolvedPath(web, fd, parent)) {
        close(fd);
        fd = -1;
    }
    rFree(parent);
    return fd;
}

/* Return 1 for a validated regular file, 0 for an absent leaf, and -1 for an unsafe target. */
static int checkFileTarget(Web *web, int dir, cchar *path, FileInfo *info)
{
    int fd, valid;

    fd = openat(dir, rBasename(path), O_RDONLY | O_BINARY | O_NONBLOCK | O_NOFOLLOW);
    if (fd < 0) {
        if (errno == ENOENT && fstatat(dir, rBasename(path), info, AT_SYMLINK_NOFOLLOW) < 0 && errno == ENOENT) {
            return 0;
        }
        return -1;
    }
    valid = fstat(fd, info) == 0 && S_ISREG(info->st_mode) && validateResolvedPath(web, fd, path);
    close(fd);
    return valid ? 1 : -1;
}

static bool filePreconditions(Web *web, FileInfo *info)
{
    char etag[24];

    fileEtag(web, info, etag, sizeof(etag));
    return (!web->ifMatchPresent || webMatchEtag(web, etag)) &&
           (!web->ifUnmodified || webMatchModified(web, info->st_mtime));
}
#endif

static int putFile(Web *web, char *path)
{
#if ME_WEB_MUTATIONS
    FileInfo info;
    char     *ptr, *parent, temp[64];
    uchar    random[16];
    size_t   bufsize;
    ssize    nbytes, total;
    int      dir, fd, exists, rc, i;

    if ((dir = openFileParent(web, path)) < 0) {
        return webError(web, 404, "Cannot open document directory");
    }
    exists = checkFileTarget(web, dir, path, &info);
    if (exists < 0) {
        close(dir);
        return webError(web, 404, "Cannot open document");
    }
    web->exists = exists ? 1 : 0;
    if (exists && !filePreconditions(web, &info)) {
        close(dir);
        return webError(web, 412, "Precondition not satisfied");
    }
    /* Never truncate the old file: publish the complete body with an atomic rename. */
    if (cryptGetRandomBytes(random, sizeof(random), 0) < 0) {
        close(dir);
        return webError(web, 500, "Cannot create temporary document");
    }
    scopy(temp, sizeof(temp), ".web-put-");
    for (i = 0; i < (int) sizeof(random); i++) {
        sfmtbuf(&temp[9 + i * 2], sizeof(temp) - (size_t) (9 + i * 2), "%02x", random[i]);
    }
    fd = openat(dir, temp, O_WRONLY | O_BINARY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (fd < 0) {
        close(dir);
        return webError(web, 404, "Cannot open document");
    }
    total = 0;
    rc = 0;
    bufsize = min(WEB_BUF_BOOST_16X, (size_t) web->rxRemaining);
    while ((nbytes = webReadDirect(web, &ptr, bufsize)) > 0) {
        total += nbytes;
        if (total > web->host->maxUpload) {
            rc = 413;
            break;
        }
        if (write(fd, ptr, (uint) nbytes) != nbytes) {
            rc = 500;
            break;
        }
    }
    if (close(fd) < 0 && !rc) {
        rc = 500;
    }
    if (!rc && (nbytes < 0 || web->rxRemaining > 0)) {
        rc = 500;
    }
    /* The body read yields: check the held directory and destination again before publishing. */
    parent = rDirname(sclone(path));
    if (!rc && !validateResolvedPath(web, dir, parent)) {
        rc = 404;
    }
    rFree(parent);
    if (!rc && checkFileTarget(web, dir, path, &info) < 0) {
        rc = 404;
    }
    if (!rc && renameat(dir, temp, dir, rBasename(path)) < 0) {
        rc = 500;
    }
    unlinkat(dir, temp, 0);
    close(dir);
    if (rc) {
        return webError(web, rc, "Cannot complete PUT document");
    }
    return (int) webWriteResponseString(web, web->exists ? 204 : 201, "Document successfully updated");
#else
    (void) path;
    return webError(web, 405, "Safe filesystem mutations are unavailable on this platform");
#endif
}

static int deleteFile(Web *web, char *path)
{
#if ME_WEB_MUTATIONS
    FileInfo info;
    int      dir, rc;

    if ((dir = openFileParent(web, path)) < 0) {
        return webError(web, 404, "Cannot locate document directory");
    }
    if (checkFileTarget(web, dir, path, &info) != 1) {
        close(dir);
        return webError(web, 404, "Cannot locate document");
    }
    if (!filePreconditions(web, &info)) {
        close(dir);
        return webError(web, 412, "Precondition not satisfied");
    }
    rc = unlinkat(dir, rBasename(path), 0);
    close(dir);
    if (rc < 0) {
        return webError(web, 404, "Cannot delete document");
    }
    return (int) webWriteResponseString(web, 204, "Document successfully deleted");
#else
    (void) path;
    return webError(web, 405, "Safe filesystem mutations are unavailable on this platform");
#endif
}

/*
    Send file content to the client using zero-copy sendfile if available.
    Supports partial file sends via offset and length parameters.
 */
PUBLIC ssize webSendFile(Web *web, int fd, Offset offset, ssize len)
{
    ssize  written, nbytes, toRead;
    size_t bufSize;
    char   *buf;

    if (len <= 0) {
        return 0;
    }
    if (!web->wroteHeaders && webWriteHeaders(web) < 0) {
        return R_ERR_CANT_WRITE;
    }
#if ME_HTTP_SENDFILE
    // Chunked responses must pass through webWrite so each file block gets its framing.
    if (web->txLen >= 0 && !rIsSocketSecure(web->sock)) {
        written = rSendFile(web->sock, fd, offset, (size_t) len, web->deadline);
        if (written < 0 || written < len) {
            return webNetError(web, "Cannot send file");
        }
        return written;
    }
#endif
    /* Headers are committed; close on failure to avoid completing a response shorter than its declared length. */
    if (offset > 0 && lseek(fd, (off_t) offset, SEEK_SET) != offset) {
        return webNetError(web, "Cannot seek in file");
    }
    bufSize = len < ME_BUFSIZE ? ME_BUFSIZE : WEB_BUF_BOOST_4X;
    buf = rAlloc(bufSize);
    for (written = 0; written < len; ) {
        toRead = min(len - written, (ssize) bufSize);
        if ((nbytes = read(fd, buf, (uint) toRead)) < 0) {
            rFree(buf);
            return webNetError(web, "Cannot read document");
        }
        if (nbytes == 0) {
            rFree(buf);
            return webNetError(web, "Premature end of input");
        }
        if ((nbytes = webWrite(web, buf, (size_t) nbytes)) < 0) {
            rFree(buf);
            return webNetError(web, "Cannot send file");
        }
        written += nbytes;
    }
    rFree(buf);
    return written;
}

/************************************ Ranges **********************************/
/*
    Fix up range offsets based on actual file size
    Convert negative offsets to positive
    Validate ranges are within file bounds
    Returns 0 if ranges are valid, error code on failure
 */
static int fixRanges(Web *web, int64 fileSize)
{
    WebRange *range;

    if (!web->ranges) {
        return 0;
    }
    for (range = web->ranges; range; range = range->next) {
        //  Fix suffix range (-500 means last 500 bytes)
        if (range->start < 0) {
            range->start = fileSize - range->end;
            if (range->start < 0) {
                range->start = 0;
            }
            range->end = fileSize;
        }
        //  Fix open-ended range (500- means from 500 to end)
        if (range->end < 0) {
            range->end = fileSize;
        }
        //  Clamp to file size
        if (range->end > fileSize) {
            range->end = fileSize;
        }
        //  Validate range
        if (range->start >= fileSize) {
            //  Range not satisfiable
            return R_ERR_BAD_REQUEST;
        }
        range->len = range->end - range->start;
    }
    coalesceRanges(web);
    return 0;
}

/*
    Merge overlapping and adjacent ranges so the bytes served never exceed the file. RFC 9110
    14.1.2 permits coalescing; without it "0-99,0-99,..." serves the same bytes once per range.
    The ranges are sorted by start first, which also normalises the order clients see. Ranges are
    absolute by the time this runs.
 */
static void coalesceRanges(Web *web)
{
    WebRange *range, *next, *sorted, *prev, *cur;

    sorted = NULL;
    for (range = web->ranges; range; range = next) {
        next = range->next;
        range->next = NULL;
        //  Insertion sort by start offset
        for (prev = NULL, cur = sorted; cur && cur->start <= range->start; prev = cur, cur = cur->next) {
        }
        range->next = cur;
        if (prev) {
            prev->next = range;
        } else {
            sorted = range;
        }
    }
    for (range = sorted; range && range->next; ) {
        next = range->next;
        if (next->start <= range->end) {
            //  Overlapping or adjacent: absorb the next range
            if (next->end > range->end) {
                range->end = next->end;
                range->len = range->end - range->start;
            }
            range->next = next->next;
            rFree(next);
        } else {
            range = next;
        }
    }
    web->ranges = sorted;
    web->currentRange = sorted;
}

/*
    Process and validate range requests
    Returns 0 on success, error code on failure
 */
static int pickRanges(Web *web, FileInfo *info, cchar *etag)
{
    WebRange *range;
    bool     rangeValid;

    /*
        Reject files beyond MAXSSIZE to prevent response and range length truncation on ILP32.
     */
    if ((int64) info->st_size > (int64) MAXSSIZE) {
        return R_ERR_WONT_FIT;
    }
    if (!web->ranges) {
        //  Default case: no ranges requested - serve full file
        web->status = 200;
        web->txLen = (ssize) info->st_size;
        return 0;
    }
    /*
        Validate If-Range precondition per RFC 7233 section 3.2
        If If-Range is present, only serve ranges if the condition matches
        Otherwise, ignore Range header and serve full content
     */
    if (web->ifRange) {
        rangeValid = false;
        if (web->ifMatch) {
            //  If-Range with ETag - check if it matches current ETag
            rangeValid = smatch(web->ifMatch, etag);
        } else if (web->since > 0) {
            //  If-Range with date - check if resource hasn't been modified since
            rangeValid = (info->st_mtime <= web->since);
        }
        //  If condition doesn't match, ignore ranges and serve full content
        if (!rangeValid) {
            //  Free the list rather than unlinking it: webFreeRanges is its only owner
            webFreeRanges(web);
            web->status = 200;
            web->txLen = (ssize) info->st_size;
            return 0;
        }
    }
    //  Validate and fix ranges based on file size
    if (fixRanges(web, info->st_size) < 0) {
        webAddHeader(web, "Content-Range", "bytes */%lld", (int64) info->st_size);
        return R_ERR_BAD_REQUEST;
    }
    web->status = 206;

    if (web->ranges->next != NULL) {
        //  Multiple ranges - use multipart/byteranges
        rFree(web->rangeBoundary);
        if ((web->rangeBoundary = cryptID(16)) == 0) {
            return R_ERR_CANT_COMPLETE;
        }
        rFree(web->rmime);
        web->mime = web->rmime = sfmt("multipart/byteranges; boundary=%s", web->rangeBoundary);
        web->txLen = -1;  // Unknown length for chunked encoding
    } else {
        //  Single range - set Content-Range header
        range = web->ranges;
        webAddHeader(web, "Content-Range", "bytes %lld-%lld/%lld", web->ranges->start, range->end - 1,
                     (int64) info->st_size);
        web->txLen = (ssize) range->len;
    }
    return 0;
}

/*
    Write Content-Range header for multipart boundary
 */
static void writeRangeHeader(Web *web, WebRange *range, int64 fileSize)
{
    cchar *mime;

    //  web->mime is the multipart envelope's type. Each part carries the document's type.
    mime = web->ext ? rLookupName(web->host->mimeTypes, web->ext) : 0;
    webWriteFmt(web, "\r\n--%s\r\n", web->rangeBoundary);
    webWriteFmt(web, "Content-Type: %s\r\n", mime ? mime : "application/octet-stream");
    webWriteFmt(web, "Content-Range: bytes %lld-%lld/%lld\r\n\r\n", range->start, range->end - 1, fileSize);
}

/*
    Send file content (ranges or full file)
    Returns 0 on success, error code on failure
 */
static int sendFileContent(Web *web, int fd, FileInfo *info)
{
    WebRange *range;

    if (web->ranges) {
        //  Send ranges
        for (range = web->ranges; range; range = range->next) {
            if (web->ranges->next != NULL) {
                //  Multipart - write range header
                writeRangeHeader(web, range, info->st_size);
            }
            if (webSendFile(web, fd, range->start, (ssize) range->len) < 0) {
                return R_ERR_CANT_WRITE;
            }
        }
        if (web->ranges->next != NULL) {
            //  Send final multipart boundary
            if (webWriteFmt(web, "\r\n--%s--\r\n", web->rangeBoundary) < 0) {
                return R_ERR_CANT_WRITE;
            }
        }
    } else {
        //  Send entire file
        if (web->txLen > 0 && webSendFile(web, fd, 0, web->txLen) < 0) {
            return R_ERR_CANT_WRITE;
        }
    }
    return 0;
}

/********************************** Compression *********************************/
/*
    Parse Accept-Encoding header and determine preferred compression
    Returns: "br", "gzip", or NULL (no compression supported/preferred)
    Preference order: br > gzip (Brotli offers better compression)
 */
static cchar *getEncoding(Web *web)
{
    cchar *acceptEncoding;
    bool  supportsBr, supportsGzip;

    if ((acceptEncoding = webGetHeader(web, "Accept-Encoding")) == 0) {
        return NULL;
    }
    /*
        Prefer brotli (better compression) if client supports it
     */
    supportsBr = scontains(acceptEncoding, "br") != NULL;
    if (supportsBr) {
        return "br";
    }
    supportsGzip = scontains(acceptEncoding, "gzip") != NULL;
    if (supportsGzip) {
        return "gzip";
    }
    return NULL;
}

/*
    Select pre-compressed file if available and client supports it
    Modifies path in-place to point to compressed variant if available
    Sets *encoding to compression type ("br" or "gzip") if compressed file selected
    Returns true if file exists, false otherwise
 */
static bool pickFile(Web *web, char *path, size_t pathSize, FileInfo *info, cchar **encoding)
{
    size_t len;
    bool   found;

    assert(encoding);

    found = 0;
    *encoding = NULL;

    /*
        Internal redirect to the directory index
        For directory index, disable compression (avoid double-checking)
     */
    if (sends(path, "/")) {
        if (sncat(path, pathSize, web->host->index) < 0) {
            return false;
        }
        //  The deny patterns ran on the directory name. The index leaf must pass them too.
        if (deniedPath(web, &path[slen(webGetDocs(web->host))])) {
            return false;
        }
        web->exists = stat(path, info) == 0;
        web->ext = strrchr(web->host->index, '.');
    }
    len = slen(path);

    if (web->route->compressed && (*encoding = getEncoding(web)) != 0) {
        if (smatch(*encoding, "br")) {
            if (sncat(path, pathSize, ".br") >= 0) {
                found = stat(path, info) == 0;
            }
        }
        if (!found && smatch(*encoding, "gzip")) {
            if (sncat(path, pathSize, ".gz") >= 0) {
                found = stat(path, info) == 0;
            }
        }
    }
    if (found) {
        web->exists = 1;
    } else {
        // Remove failed .br, .gz extensions
        path[len] = '\0';
        *encoding = NULL;
        web->exists = stat(path, info) == 0;
        if (web->exists && S_ISDIR(info->st_mode)) {
            // External redirect to the directory
            redirectToDir(web);
            return 0;
        }
    }
    return web->exists;
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file src/host.c ************/

/*
    host.c - Web Host. This is responsible for a set of listening endpoints.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/


#if ME_WIN_LIKE
//  ConvertSidToStringSidA, for naming the owner in a diagnostic
    #include <sddl.h>
//  SetEntriesInAclA and EXPLICIT_ACCESS_A, to create a directory with an explicit DACL
    #include <aclapi.h>
#endif

/************************************ Forwards *********************************/

static WebListen *allocListen(WebHost *host, cchar *endpoint);
static RHash *createMethodsHash(cchar *list);
static void freeListen(WebListen *listen);
static int getLimit(WebHost *host, cchar *field, cchar *defaultValue);
static void closeConnections(WebHost *host);
static void pruneListeners(WebHost *host);

/*
    How long webFreeHost waits for connection fibers resumed by closing their sockets to finish
 */
#define WEB_DRAIN_TIMEOUT (2 * TPS)
static Ticks getTimeout(WebHost *host, cchar *field, cchar *defaultValue);
static void initDeny(WebHost *host);
static void initMethods(WebHost *host);
static void initRedirects(WebHost *host);
static void initRoutes(WebHost *host);
static void loadMimeTypes(WebHost *host);
static int loadAuth(WebHost *host);
static int checkRouteFeatures(WebHost *host);
static bool supportedAuthType(cchar *authType);
static void parseCacheControl(WebRoute *route, Json *json, int id);
static void checkRouteKeys(cchar *match, Json *json, int id);
#if ME_WEB_UPLOAD
static int checkUploadDir(WebHost *host);
#endif
static bool detectCaseFolding(cchar *documents);
static char *flipCase(cchar *name);

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
    WebHost     *host;
    cchar       *path, *show;
    char        *errorMsg;
    struct stat docsInfo;

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
    host->connSequence = 0;

    if (!config) {
        if ((config = jsonParseFile(ME_WEB_CONFIG, &errorMsg, 0)) == 0) {
            rError("config", "%s", errorMsg);
            rFree(errorMsg);
            webFreeHost(host);
            return 0;
        }
        jsonLock(config);
        host->freeConfig = 1;
    }
    host->config = config;

    /*
        A "web.web" node is unambiguously the double-nesting mistake: a whole web.json5, which is
        already rooted at "web", pasted under another "web" key. Every setting below it is then
        invisible - including the route table, which leaves the host with the fail-closed catch-all
        installed by initRoutes rather than the configured routes. Say so rather than reading none
        of it.
     */
    if (jsonGetNode(host->config, 0, "web.web") != 0) {
        rError("web", "Configuration has a nested 'web.web' node and everything under it is ignored. "
               "The web configuration is the contents of the 'web' object, not another 'web' object.");
    }

    /*
        Parse a signatures.json file that is used to validate REST requests to the web server
     */
    if (jsonGetBool(host->config, 0, "web.signatures.enable", 0)) {
        path = jsonGet(host->config, 0, "web.signatures.path", 0);
        if ((host->signatures = jsonParseFile(path, &errorMsg, 0)) == 0) {
            rError("web", "Cannot parse signatures file: %s", errorMsg);
            rFree(errorMsg);
            webFreeHost(host);
            return 0;
        }
        host->strictSignatures = jsonGetBool(host->config, 0, "web.signatures.strict", 0);
    }

    host->index = jsonGet(host->config, 0, "web.index", "index.html");
    host->parseTimeout = getTimeout(host, "web.timeouts.parse", "5secs");
    /*
        The whole of a buffered request body must arrive within this, measured from the start of the
        request and not refreshed by activity. inactivity and request are both refreshed - the first
        by every byte, the second is simply long - so a peer that drips one byte at a time held its
        fiber, and therefore one of the pool's connection slots, for the full request timeout while
        sending almost nothing. It bounds only webReadBody, which is the body the server buffers for
        a handler and is already capped at web.limits.body; streamed routes, PUT and uploads read at
        their own pace and are deliberately not covered.
     */
    host->receiveTimeout = getTimeout(host, "web.timeouts.receive", "30secs");
    host->inactivityTimeout = getTimeout(host, "web.timeouts.inactivity", "5mins");
    host->requestTimeout = getTimeout(host, "web.timeouts.request", "5mins");
    host->sessionTimeout = getTimeout(host, "web.timeouts.session", "30mins");

    host->maxDigest = getLimit(host, "web.limits.digest", "1000");
    host->maxBuffer = getLimit(host, "web.limits.buffer", "64K");
    host->maxBody = getLimit(host, "web.limits.body", "100K");
    host->maxConnections = getLimit(host, "web.limits.connections", "100");
    host->maxHeader = getLimit(host, "web.limits.header", "10K");
    host->maxSessions = getLimit(host, "web.limits.sessions", "100");
    host->maxUpload = getLimit(host, "web.limits.upload", "20MB");
    host->maxUploads = getLimit(host, "web.limits.uploads", "50");
    host->maxRequests = getLimit(host, "web.limits.requests", "1000");
    host->maxVars = getLimit(host, "web.limits.vars", "1024");
    /* Keep buffered body limits below ME_R_MAX_BUF with room for the terminator. */
    if (host->maxBody > ME_R_MAX_BUF - 1) {
        rInfo("web", "web.limits.body is limited to %d bytes", ME_R_MAX_BUF - 1);
        host->maxBody = ME_R_MAX_BUF - 1;
    }

    initDeny(host);
    cryptGetRandomBytes(host->etagSalt, sizeof(host->etagSalt), 0);
    host->docs = rGetFilePath(jsonGet(host->config, 0, "web.documents", "@site"));
    /* Fail startup rather than silently exporting the filesystem or accepting an unusable root. */
    host->canonicalDocs = rGetRealPath(host->docs);
    if (!host->docs || !*host->docs) {
        rError("web", "web.documents is empty");
        webFreeHost(host);
        return 0;
    }
    if (!host->canonicalDocs || stat(host->canonicalDocs, &docsInfo) < 0 || !S_ISDIR(docsInfo.st_mode)) {
        rError("web", "web.documents \"%s\" is not an accessible directory", host->docs);
        webFreeHost(host);
        return 0;
    }
    if (smatch(host->canonicalDocs, "/") || smatch(host->canonicalDocs, "\\") ||
        (slen(host->canonicalDocs) == 3 && host->canonicalDocs[1] == ':')) {
        rError("web", "web.documents must not be a filesystem root");
        webFreeHost(host);
        return 0;
    }
    host->foldCase = detectCaseFolding(host->canonicalDocs);
    host->name = jsonGet(host->config, 0, "web.name", 0);
#if ME_WEB_UPLOAD
    host->uploadDir = rGetFilePath(jsonGet(host->config, 0, "web.upload.dir", "@state/uploads"));
#endif
    host->sessionCookie = jsonGet(host->config, 0, "web.sessions.cookie", WEB_SESSION_COOKIE);
    host->sameSite = jsonGet(host->config, 0, "web.sessions.sameSite", "Lax");
    host->httpOnly = jsonGetBool(host->config, 0, "web.sessions.httpOnly", 1);
    host->roles = jsonGetId(host->config, 0, "web.auth.roles");
    host->headers = jsonGetId(host->config, 0, "web.headers");
    host->corsOrigins = jsonGetId(host->config, 0, "web.cors.origins");
#if ME_WEB_FIBER_BLOCKS
    // Defaults to false
    host->fiberBlocks = jsonGetBool(host->config, 0, "web.fiberBlocks", 0);
#endif

    host->hosts = jsonGetId(host->config, 0, "web.hosts");
#if ME_COM_WEBSOCK
    host->webSocketsMaxMessage = getLimit(host, "web.limits.maxMessage", "100K");
    host->webSocketsMaxFrame = getLimit(host, "web.limits.maxFrame", "100K");
    host->webSocketsPingPeriod = svaluei(jsonGet(host->config, 0, "web.webSockets.ping", "never"));
    host->webSocketsProtocol = jsonGet(host->config, 0, "web.webSockets.protocol", "chat");
    host->webSocketsOrigins = jsonGetId(host->config, 0, "web.webSockets.origins");
    host->webSocketsEnable = jsonGetBool(host->config, 0, "web.webSockets.enable", 1);
    host->webSocketsValidateUTF = jsonGetBool(host->config, 0, "web.webSockets.validateUTF", 0);
#endif

    initMethods(host);
    initRoutes(host);
    initRedirects(host);
    loadMimeTypes(host);
    if (loadAuth(host) < 0) {
        webFreeHost(host);
        return 0;
    }
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
    Ticks       deadline;
    int         next;

    rStopEvent(host->sessionEvent);

    /*
        Close sockets to resume connection fibers, then wait boundedly for queued resumes to unwind before freeing
        connections.
     */
    closeConnections(host);
    if (!rIsMain()) {
        for (deadline = rGetTicks() + WEB_DRAIN_TIMEOUT; rGetListLength(host->webs) > 0 && rGetTicks() < deadline; ) {
            rSleep(10);
        }
    }
    /*
        Drain the list because webFree shifts its remaining elements.
     */
    while ((web = rGetItem(host->webs, 0)) != 0) {
        webFree(web);
    }
    //  After the connections: a connection's disconnect path reads its listener
    for (ITERATE_ITEMS(host->listeners, listen, next)) {
        freeListen(listen);
    }
    rFreeList(host->listeners);
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
        rFreeHash(route->extensions);
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
    rFreeHash(host->authFailures);

#if ME_WEB_HTTP_AUTH
    // Free HTTP authentication configuration (realm, authType, algorithm come from config - not cloned)
    rFree(host->secret);
    rFree(host->opaque);
    rFree(host->decoyPassword);
#if ME_WEB_AUTH_BASIC
    rFree(host->dummyUser.password);
#endif
#if ME_WEB_AUTH_DIGEST
    rFree(host->dummyDigestPassword);
    // Free nonce tracking hash table and stop cleanup timer
    if (host->nonceCleanupEvent) {
        rStopEvent(host->nonceCleanupEvent);
    }
    if (host->nonces) {
        rFreeHash(host->nonces);
    }
    if (host->nonceUsers) {
        rFreeHash(host->nonceUsers);
    }
#endif
#endif

    // Free users hash (always available for session-based auth)
    if (host->users) {
        for (ITERATE_NAMES(host->users, np)) {
            webFreeUser((WebUser*) np->value);
            np->value = NULL;
        }
        rFreeHash(host->users);
    }
    if (host->freeConfig) {
        jsonFree(host->config);
    }
    if (host->signatures) {
        jsonFree(host->signatures);
        host->signatures = 0;
    }
    rFreeList(host->deny);
    rFree(host->docs);
#if ME_WEB_UPLOAD
    rFree(host->uploadDir);
#endif
    rFree(host->canonicalDocs);
    rFree(host->ip);
    rFree(host);
}

PUBLIC int webGetFiberLimit(Json *config, int reserve)
{
    int maxConnections, maxFibers;

    maxConnections = svaluei(jsonGet(config, 0, "web.limits.connections", "100"));
    maxFibers = svaluei(jsonGet(config, 0, "limits.fibers", "0"));
    if (maxFibers == 0) {
        maxFibers = maxConnections + reserve;
    } else if (maxFibers < maxConnections + reserve) {
        rError("web",
               "limits.fibers is %d but web.limits.connections is %d. The fiber pool is the real "
               "ceiling, so connections beyond it are dropped without a response. Raise limits.fibers "
               "to %d, or lower web.limits.connections.",
               maxFibers, maxConnections, maxConnections + reserve);
    }
    return maxFibers;
}

PUBLIC int webStartHost(WebHost *host)
{
    Json      *json;
    WebListen *listen;
    JsonNode  *np;
    cchar     *endpoint;

    if (!host || !host->listeners) return 0;
#if ME_WEB_UPLOAD
    if (checkUploadDir(host) < 0) {
        return R_ERR_CANT_OPEN;
    }
#endif
    json = host->config;
    pruneListeners(host);

    for (ITERATE_JSON_KEY(json, 0, "web.listen", np, id)) {
        endpoint = jsonGet(json, id, 0, 0);
        if ((listen = allocListen(host, endpoint)) == 0) {
            //  A failed start leaves the host stopped, not serving a subset of its endpoints
            webStopHost(host);
            pruneListeners(host);
            return R_ERR_CANT_OPEN;
        }
        rAddItem(host->listeners, listen);
    }
    webInitSessions(host);
    return 0;
}

PUBLIC void webStopHost(WebHost *host)
{
    WebListen *listen;
    int       next;

    rStopEvent(host->sessionEvent);

    for (ITERATE_ITEMS(host->listeners, listen, next)) {
        rCloseSocket(listen->sock);
    }
    closeConnections(host);
}

/* Walk backwards because closing can resume a fiber that removes its connection from host->webs. */
static void closeConnections(WebHost *host)
{
    Web *web;
    int i;

    for (i = rGetListLength(host->webs) - 1; i >= 0; i--) {
        if (i < rGetListLength(host->webs) && (web = rGetItem(host->webs, i)) != 0) {
            rCloseSocket(web->sock);
        }
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
    int64     value;
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
    //  Strict: a sign, white space or a value that only fits after truncation is not a port
    if (sparseUint(sport, 10, 65535, &end, &value) < 0 || *end || value == 0) {
        rError("web", "Bad or missing port \"%s\" in endpoint \"%s\"", sport, endpoint);
        rFree(tok);
        return 0;
    }
    port = (int) value;
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

    if (smatch(scheme, "https")) {
#if ME_COM_SSL
        if (webSecureEndpoint(listen) < 0) {
            /*
                Refuse to listen rather than accept connections that can never complete a
                handshake. A device that cannot serve TLS must not come up pretending it can,
                especially where a plaintext listener is also configured and would silently
                carry the traffic instead.
             */
            rError("web", "Cannot secure endpoint %s - not listening", listen->endpoint);
            freeListen(listen);
            rFree(tok);
            return 0;
        }
#else
        //  Without a TLS stack an https listener would serve plaintext on the TLS port
        rError("web", "TLS is not supported in this build - not listening on %s", listen->endpoint);
        freeListen(listen);
        rFree(tok);
        return 0;
#endif
    }
    if (rListenSocket(sock, hostname, port, (RSocketProc) webAlloc, listen) < 0) {
        rError("web", "Cannot listen on %s:%d", hostname ? hostname : "*", port);
        freeListen(listen);
        rFree(tok);
        return 0;
    }
    rFree(tok);
    return listen;
}

/*
    Free stopped listeners unless a remaining connection still references them.
 */
static void pruneListeners(WebHost *host)
{
    WebListen *listen;
    Web       *web;
    int       i, next;
    bool      inUse;

    for (i = rGetListLength(host->listeners) - 1; i >= 0; i--) {
        listen = rGetItem(host->listeners, i);
        if (!rIsSocketClosed(listen->sock)) {
            continue;
        }
        inUse = 0;
        for (ITERATE_ITEMS(host->webs, web, next)) {
            if (web->listen == listen) {
                inUse = 1;
                break;
            }
        }
        if (!inUse) {
            rRemoveItemAt(host->listeners, i);
            freeListen(listen);
        }
    }
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
    char  *authority, *certificate, *key, *revoke;
    bool  verifyClient;
    int   rc, verifyIssuer;

    config = listen->host->config;

    /*
        Configure this listener's own socket. These must NOT be set as process-wide defaults.

        "tls.verify.client" asks whether this server requires and verifies a client certificate
        (mutual TLS), and it is correctly false by default because few deployments want mTLS.
        The process-wide default governs something different and nearly opposite: whether an
        OUTBOUND socket verifies the certificate of the server it connects to. Routing a server
        setting into that global turned "verify peers by default" into "verify nothing" for the
        whole process the moment an https listener was configured - disabling certificate
        verification on cloud sync, MQTT, the URL client and any integrator update channel.

        rAcceptTls copies verifyPeer/verifyIssuer from the listener to each accepted connection,
        so setting them here reaches every inbound connection and nothing else.
     */
    ciphers = jsonGet(config, 0, "tls.ciphers", 0);
    if (ciphers) {
        char *clist = jsonToString(config, 0, "tls.ciphers", JSON_BARE);
        rSetSocketCiphers(listen->sock, clist);
        rFree(clist);
    }
    verifyClient = jsonGetBool(config, 0, "tls.verify.client", 0);
    /*
        An absent "tls.verify.issuer" must inherit the runtime default, which verifies. Reading it
        as a plain boolean wrote an explicit 0 over the runtime's -1 "inherit", and the verify
        callback waives every chain-establishment error when issuer verification is off - so a
        listener that required a client certificate accepted one the caller had generated seconds
        earlier, chaining to nothing. tls.authority is loaded and even sent as the CA hint list, so
        nothing about the configuration suggests it is not being enforced.
     */
    verifyIssuer = jsonGet(config, 0, "tls.verify.issuer", 0) == 0 ? -1 :
                   (jsonGetBool(config, 0, "tls.verify.issuer", 1) ? 1 : 0);
    if (verifyClient && verifyIssuer == 0) {
        /*
            Refuse rather than silently upgrade: a client certificate that chains to nothing is not
            an identity, and an operator who wants any certificate at all should have to say so
            somewhere this code can see it.
         */
        rError("web", "tls.verify.client requires tls.verify.issuer: a client certificate that "
               "does not chain to tls.verify.authority is not an identity");
        return R_ERR_BAD_ARGS;
    }
    /*
        Use a dedicated client-certificate authority. Outbound roots or system public CAs must not grant mTLS
        identities.
     */
    authority = verifyClient ? rGetFilePath(jsonGet(config, 0, "tls.verify.authority", 0)) : NULL;
    if (verifyClient && !authority) {
        rError("web", "tls.verify.client requires tls.verify.authority: the CA that issues client certificates");
        return R_ERR_BAD_ARGS;
    }
    rSetSocketVerify(listen->sock, verifyClient, verifyIssuer);

    //  Revoked client certificates. Only meaningful when client certificates are verified.
    revoke = verifyClient ? rGetFilePath(jsonGet(config, 0, "tls.verify.revoke", 0)) : NULL;

    certificate = rGetFilePath(jsonGet(config, 0, "tls.certificate", 0));
    key = rGetFilePath(jsonGet(config, 0, "tls.key", 0));

    rc = 0;
    if (!key || !certificate) {
        rError("web", "Secure endpoint %s has no tls.certificate or tls.key configured", listen->endpoint);
        rc = R_ERR_CANT_OPEN;
    } else {
        if (rAccessFile(key, R_OK) < 0) {
            rError("web", "Cannot access certificate key %s", key);
            rc = R_ERR_CANT_OPEN;
        } else if (rAccessFile(certificate, R_OK) < 0) {
            rError("web", "Cannot access certificate %s", certificate);
            rc = R_ERR_CANT_OPEN;
        } else if (authority && rAccessFile(authority, R_OK) < 0) {
            rError("web", "Cannot access authority %s", authority);
            rc = R_ERR_CANT_OPEN;
        } else if (revoke && rAccessFile(revoke, R_OK) < 0) {
            rError("web", "Cannot access certificate revocation list %s", revoke);
            rc = R_ERR_CANT_OPEN;
        }
    }
    if (rc == 0) {
        rSetSocketCerts(listen->sock, authority, key, certificate, revoke);
    }
    rFree(revoke);
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
/*
    Use the default for negative limits; clamp "unlimited" to MAXINT.
 */
static int getLimit(WebHost *host, cchar *field, cchar *defaultValue)
{
    int64 value;

    value = svalue(jsonGet(host->config, 0, field, defaultValue));
    if (value < 0) {
        rError("web", "Configuration %s must not be negative; using default %s", field, defaultValue);
        value = svalue(defaultValue);
    }
    return (int) min(value, MAXINT);
}

static Ticks getTimeout(WebHost *host, cchar *field, cchar *defaultValue)
{
    int64 value;

    /*
        Timeouts are configured in seconds and stored as Ticks (milliseconds).
        Clamp so the conversion to milliseconds cannot overflow.
     */
    value = svalue(jsonGet(host->config, 0, field, defaultValue));
    if (value < 0) {
        rError("web", "Configuration %s must not be negative; using default %s", field, defaultValue);
        value = svalue(defaultValue);
    } else if (value > MAXINT64 / TPS) {
        value = MAXINT64 / TPS;
    }
    return (Ticks) value * TPS;
}

#if ME_WEB_UPLOAD
/*
    On Windows, mode bits do not describe directory access. Check the owner and the DACL,
    permitting write access only to the process user, Administrators and LocalSystem.
 */
#if ME_WIN_LIKE
/*
    Name the SID for a diagnostic. The caller supplies the buffer: a message naming two SIDs
    evaluates both before formatting either, so a shared buffer would print one twice.
 */
static cchar *sidName(SID *sid, char *buf, size_t bufsize)
{
    char *text;

    if (sid && ConvertSidToStringSidA(sid, &text)) {
        scopy(buf, bufsize, text);
        LocalFree(text);
        return buf;
    }
    return "unknown";
}

/*
    Six distinct conditions used to share one message, which told an operator nothing they could
    act on. Each now names itself.
 */
static bool privateUploadDir(cchar *path, int uid)
{
    SECURITY_DESCRIPTOR *security;
    TOKEN_USER          *user;
    ACL                 *acl;
    ACE_HEADER          *ace;
    ACCESS_ALLOWED_ACE  *allow;
    SID                 *owner;
    HANDLE              token;
    char                ownerSid[256], userSid[256];
    DWORD               size, attributes, i;
    BOOL                present, defaulted;
    bool                valid;

    (void) uid;
    security = NULL;
    user = NULL;
    token = NULL;
    valid = 0;
    attributes = GetFileAttributesA(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        rError("web", "Upload directory %s does not exist", path);
        return 0;
    }
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        rError("web", "Upload directory %s is not a directory", path);
        return 0;
    }
    if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        rError("web", "Upload directory %s is a reparse point", path);
        return 0;
    }
    size = 0;
    GetFileSecurityA(path, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, NULL, 0, &size);
    if (!size || !(security = rAlloc(size)) ||
        !GetFileSecurityA(path, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, security, size, &size) ||
        !OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        rError("web", "Cannot read the security descriptor of upload directory %s", path);
        goto done;
    }
    size = 0;
    GetTokenInformation(token, TokenUser, NULL, 0, &size);
    if (!size || !(user = rAlloc(size)) || !GetTokenInformation(token, TokenUser, user, size, &size)) {
        rError("web", "Cannot read the process token");
        goto done;
    }
    if (!GetSecurityDescriptorOwner(security, (PSID*) &owner, &defaulted) || !owner) {
        rError("web", "Upload directory %s has no owner", path);
        goto done;
    }
    /*
        The same principals the DACL check below permits to write. An owner can do nothing that the
        DACL does not already allow that principal to do, so demanding the process user alone
        rejected directories that are private by every useful measure. Windows makes
        BUILTIN\Administrators the owner of whatever a member of that group creates when the
        "default owner for objects created by members of the Administrators group" policy says so,
        which covers every directory an elevated service creates - including the one checkUploadDir
        creates for itself a few lines above.
     */
    if (!EqualSid(owner, user->User.Sid) &&
        !IsWellKnownSid(owner, WinLocalSystemSid) &&
        !IsWellKnownSid(owner, WinBuiltinAdministratorsSid)) {
        rError("web", "Upload directory %s is owned by %s, which is neither the server user %s, "
               "LocalSystem nor Administrators", path,
               sidName(owner, ownerSid, sizeof(ownerSid)), sidName((SID*) user->User.Sid, userSid, sizeof(userSid)));
        goto done;
    }
    if (!GetSecurityDescriptorDacl(security, &present, &acl, &defaulted) || !present || !acl) {
        rError("web", "Upload directory %s has no access control list", path);
        goto done;
    }
    for (i = 0; i < acl->AceCount; i++) {
        if (!GetAce(acl, i, (void**) &ace)) {
            goto done;
        }
        if (ace->AceFlags & INHERIT_ONLY_ACE) {
            continue;
        }
        if (ace->AceType == ACCESS_DENIED_ACE_TYPE) {
            continue;
        }
        if (ace->AceType != ACCESS_ALLOWED_ACE_TYPE) {
            rError("web", "Upload directory %s has an access control entry of an unsupported type", path);
            goto done;
        }
        allow = (ACCESS_ALLOWED_ACE*) ace;
        if ((allow->Mask & (GENERIC_ALL | GENERIC_WRITE | FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY |
                            FILE_DELETE_CHILD | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA | DELETE | WRITE_DAC |
                            WRITE_OWNER)) &&
            !EqualSid(&allow->SidStart, user->User.Sid) &&
            !IsWellKnownSid(&allow->SidStart, WinLocalSystemSid) &&
            !IsWellKnownSid(&allow->SidStart, WinBuiltinAdministratorsSid)) {
            rError("web", "Upload directory %s grants write access to %s", path,
                   sidName((SID*) &allow->SidStart, ownerSid, sizeof(ownerSid)));
            goto done;
        }
    }
    valid = 1;
done:
    if (token) CloseHandle(token);
    rFree(user);
    rFree(security);
    return valid;
}
#else
static bool privateUploadDir(cchar *path, int uid)
{
    struct stat info;

#if ME_UNIX_LIKE
    return lstat(path, &info) == 0 && S_ISDIR(info.st_mode) &&
           info.st_uid == (uid_t) uid && !(info.st_mode & 0022);
#else
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
#endif
}
#endif

/*
    Default to the application's state area, never the shared system temporary directory.
    Explicit directories must already exist. Reject writable-by-others directories even with
    the sticky bit: upload storage is private application state.
 */
/*
    The identity the server will actually run as.

    Binding a privileged port needs root, so a root-started server drops to web.user only after its
    listeners are up - after this check has run. Validating the upload directory against the current
    euid therefore asserted a property of an identity the server was about to stop having: a
    root-owned directory passed startup and then failed every write for the life of the process,
    while one owned by the runtime user failed startup outright.

    Only root acts on web.user, so an embedder that never drops privileges is unaffected and the
    library is told the identity rather than guessing it.
 */
static int runtimeUid(WebHost *host)
{
#if ME_UNIX_LIKE
    struct passwd *pp;
    cchar         *user;

    //  The same default the web command drops to, so the upload directory is owned by that account
    if (geteuid() == 0 && (user = jsonGet(host->config, 0, "web.user", ME_WEB_USER)) != 0 && *user &&
        (pp = getpwnam(user)) != 0) {
        return (int) pp->pw_uid;
    }
    return (int) geteuid();
#else
    (void) host;
    return 0;
#endif
}

#if ME_WIN_LIKE
/*
    Grant one principal full control, inherited by everything created underneath.
 */
static void grantFullControl(EXPLICIT_ACCESS_A *ea, PSID sid)
{
    ea->grfAccessPermissions = GENERIC_ALL;
    ea->grfAccessMode = SET_ACCESS;
    ea->grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    ea->Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea->Trustee.TrusteeType = TRUSTEE_IS_UNKNOWN;
    ea->Trustee.ptstrName = (LPSTR) sid;
}

/*
    Create a directory carrying an explicit DACL that grants only the process user, LocalSystem and
    Administrators - the same three principals privateUploadDir accepts.

    osdep.h maps mkdir(a, b) to _mkdir(a) and discards the mode, documented there as an acceptable
    tradeoff, so a directory created that way simply inherits its parent's ACEs. Where the parent
    grants write to Users - a build agent workspace, or a directory created off a drive root - the
    server created a directory that its own privateUploadDir then correctly refused, and so refused
    to start. Fix the creation, not the check: accepting an inherited Users ACE would make upload
    storage world-writable on exactly the machines where that matters.

    An existing directory is left alone and reported as usable; the caller validates it. Taking
    ownership of a directory somebody else provisioned is not this code's decision to make.
 */
static int makeRuntimeDir(cchar *path, int uid)
{
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    SECURITY_ATTRIBUTES      sa;
    SECURITY_DESCRIPTOR      sd;
    EXPLICIT_ACCESS_A        access[3];
    TOKEN_USER               *user;
    ACL                      *acl;
    HANDLE                   token;
    PSID                     system, admins;
    DWORD                    size;
    int                      rc;

    (void) uid;
    acl = NULL;
    user = NULL;
    token = NULL;
    system = NULL;
    admins = NULL;
    rc = R_ERR_CANT_OPEN;

    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
        return 0;
    }
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        rError("web", "Cannot read the process token");
        goto done;
    }
    size = 0;
    GetTokenInformation(token, TokenUser, NULL, 0, &size);
    if (!size || !(user = rAlloc(size)) || !GetTokenInformation(token, TokenUser, user, size, &size)) {
        rError("web", "Cannot read the process token user");
        goto done;
    }
    if (!AllocateAndInitializeSid(&ntAuthority, 1, SECURITY_LOCAL_SYSTEM_RID,
                                  0, 0, 0, 0, 0, 0, 0, &system) ||
        !AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admins)) {
        rError("web", "Cannot create the well-known SIDs needed to secure %s", path);
        goto done;
    }
    memset(access, 0, sizeof(access));
    grantFullControl(&access[0], user->User.Sid);
    grantFullControl(&access[1], system);
    grantFullControl(&access[2], admins);

    if (SetEntriesInAclA(3, access, NULL, &acl) != ERROR_SUCCESS || !acl) {
        rError("web", "Cannot build the access control list for %s", path);
        goto done;
    }
    if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION) ||
        !SetSecurityDescriptorDacl(&sd, TRUE, acl, FALSE)) {
        rError("web", "Cannot build the security descriptor for %s", path);
        goto done;
    }
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = &sd;
    sa.bInheritHandle = FALSE;

    if (!CreateDirectoryA(path, &sa)) {
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            rc = 0;
        } else {
            rError("web", "Cannot create directory %s", path);
        }
        goto done;
    }
    rc = 0;

done:
    if (acl) {
        LocalFree(acl);
    }
    if (system) {
        FreeSid(system);
    }
    if (admins) {
        FreeSid(admins);
    }
    rFree(user);
    if (token) {
        CloseHandle(token);
    }
    return rc;
}

#else
/*
    Create a directory that the runtime user will own. Returns 0 if it exists and is usable.
 */
static int makeRuntimeDir(cchar *path, int uid)
{
    bool created;

#if ME_UNIX_LIKE
    struct stat info;
    int         fd, rc;
#endif

    created = mkdir(path, 0700) == 0;
    if (!created && errno != EEXIST) {
        return R_ERR_CANT_OPEN;
    }
#if ME_UNIX_LIKE
    /*
        Chown only directories this call created. Use a non-symlink descriptor and verify process ownership to
        exclude parent-directory replacement races.
     */
    if (created && (uid_t) uid != geteuid()) {
        rc = R_ERR_CANT_OPEN;
        if ((fd = open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW)) >= 0) {
            if (fstat(fd, &info) == 0 && S_ISDIR(info.st_mode) && info.st_uid == geteuid() &&
                fchown(fd, (uid_t) uid, (gid_t) -1) == 0) {
                rc = 0;
            }
            close(fd);
        }
        if (rc < 0) {
            rError("web", "Cannot set ownership of %s to uid %d", path, uid);
            return rc;
        }
    }
#endif
    return 0;
}
#endif /* ME_WIN_LIKE */

static int checkUploadDir(WebHost *host)
{
    char *state;
    int  uid;

    if (!host->uploadDir) {
        return R_ERR_CANT_OPEN;
    }
    uid = runtimeUid(host);

    if (!jsonGet(host->config, 0, "web.upload.dir", NULL)) {
        state = rGetFilePath("@state");
        if (!state) {
            return R_ERR_CANT_OPEN;
        }
        /*
            The state directory has to be traversable by the runtime user for the upload directory
            inside it to be reachable at all.
         */
        if (makeRuntimeDir(state, uid) < 0 || !privateUploadDir(state, uid)) {
            rError("web", "State directory %s must be a private directory owned by the server user", state);
            rFree(state);
            return R_ERR_CANT_OPEN;
        }
        rFree(state);
        if (makeRuntimeDir(host->uploadDir, uid) < 0) {
            rError("web", "Cannot create upload directory %s", host->uploadDir);
            return R_ERR_CANT_OPEN;
        }
    }
    if (!privateUploadDir(host->uploadDir, uid)) {
        rError("web", "Upload directory %s must be an existing private directory owned by the server user",
               host->uploadDir);
        return R_ERR_CANT_OPEN;
    }
    return 0;
}
#endif /* ME_WEB_UPLOAD */

/*
    The set of methods the server accepts, from "web.methods".

    This used to be read from the CORS response header "web.headers.Access-Control-Allow-Methods".
    That header answers "what may a browser attempt cross-origin"; this hash answers "what may any
    client do here", and it is the only method gate the server has. Setting the header to satisfy a
    browser preflight therefore widened authorization for every route without its own "methods"
    array - including the file handler's PUT and DELETE over the document root.

    PUT and DELETE are not accepted from this host-wide default. They mutate the document root and
    there is no safe default under which a catch-all route should accept them, so a route must list
    them itself.
 */
static void initMethods(WebHost *host)
{
    cchar *methods;

    if (jsonGet(host->config, 0, "web.headers.Access-Control-Allow-Methods", 0) != 0) {
        rInfo("web", "The Access-Control-Allow-Methods header no longer sets the accepted methods. "
              "Use web.methods for that, and a route's own methods array for PUT and DELETE.");
    }
    methods = jsonGet(host->config, 0, "web.methods", "GET, POST");
    host->methods = createMethodsHash(methods);

    if (rLookupName(host->methods, "PUT") || rLookupName(host->methods, "DELETE")) {
        rError("web", "web.methods must not grant PUT or DELETE. They are ignored here - "
               "list them in the methods array of the routes that should accept them.");
        rRemoveName(host->methods, "PUT");
        rRemoveName(host->methods, "DELETE");
    }
}

/*
    Return a copy of name with the first alphabetic character's case flipped, or NULL if it has none.
 */
static char *flipCase(cchar *name)
{
    char *alt;
    int  i;

    alt = sclone(name);
    for (i = 0; alt[i]; i++) {
        if (isalpha((uchar) alt[i])) {
            alt[i] = islower((uchar) alt[i]) ? (char) toupper((uchar) alt[i]) : (char) tolower((uchar) alt[i]);
            return alt;
        }
    }
    rFree(alt);
    return 0;
}

/*
    Probe case folding inside the document root, whose filesystem may differ from its parent. Default to folding
    when inconclusive. Mixed case-sensitive and case-folding mounts below the root are unsupported.
 */
static bool detectCaseFolding(cchar *documents)
{
#if ME_WIN_LIKE
    /*
        NTFS preserves case and does not distinguish it, and the Win32 layer folds regardless of the
        volume. Windows also accepts 8.3 short names and trailing dots, so a byte-exact gate is
        never right here.
     */
    (void) documents;
    return 1;
#else
    struct dirent *dp;
    struct stat   info, altInfo;
    DIR           *dir;
    char          *alt, *path, *probe;
    bool          folds, probed;

    if (!documents || !*documents || (dir = opendir(documents)) == 0) {
        return 1;
    }
    folds = 1;
    probed = 0;
    while (!probed && (dp = readdir(dir)) != 0) {
        if (smatch(dp->d_name, ".") || smatch(dp->d_name, "..") || (alt = flipCase(dp->d_name)) == 0) {
            continue;
        }
        path = rJoinFile(documents, dp->d_name);
        probe = rJoinFile(documents, alt);
        if (lstat(path, &info) == 0) {
            folds = lstat(probe, &altInfo) == 0 && altInfo.st_ino == info.st_ino && altInfo.st_dev == info.st_dev;
            probed = 1;
        }
        rFree(path);
        rFree(probe);
        rFree(alt);
    }
    closedir(dir);
    return folds;
#endif
}

/*
    Filename patterns that are never served, whatever the routes say.

    The file handler serves any regular file under the document root that a route matched, and a
    route matches on the path prefix, so a filename policy cannot be expressed as a route at all.
    Anything that lands in the document tree - by a build step, a deployment script, an editor, or
    a git clone - is otherwise reachable: ".env" carries cloud credentials, and a single fetch of
    "/.git/HEAD" confirms the object store is there, after which every secret ever committed is
    retrievable.

    Matched against every path segment, so "/.git/objects/ab/cdef" is refused and not merely
    "/.git". Configuring "web.deny" replaces this list rather than adding to it, so a site that
    genuinely serves ".well-known/" can do so, but has to say so - which is the point.
 */
static cchar *defaultDeny[] = {
    ".*",                                       /* dot-files and dot-directories */
    "*~",                                       /* editor backups */
    "*.bak", "*.old", "*.orig", "*.save",       /* editor and deployment backups */
    "*.swp",                                    /* vim swap files */
    "*.map",                                    /* source maps for minified assets */
    0
};

static void initDeny(WebHost *host)
{
    JsonNode *item;
    int      denyId, i;

    host->deny = rAllocList(0, R_DYNAMIC_VALUE);

    if ((denyId = jsonGetId(host->config, 0, "web.deny")) >= 0) {
        for (ITERATE_JSON_ID(host->config, denyId, item, i)) {
            if (item->value && *item->value) {
                rAddItem(host->deny, sclone(item->value));
            }
        }
        if (rLookupStringItem(host->deny, ".*") < 0) {
            rInfo("web", "web.deny replaces the default list and omits \".*\": dot-files will be served");
        }
        return;
    }
    for (i = 0; defaultDeny[i]; i++) {
        rAddItem(host->deny, sclone(defaultDeny[i]));
    }
}

static RHash *createMethodsHash(cchar *list)
{
    RHash *hash;
    char  *method, *methods, *tok;

    methods = sclone(list);
    hash = rAllocHash(0, R_TEMPORAL_NAME);
    //  Accepts a plain list or the string form of a JSON array, so brackets are separators too
    for (method = stok(methods, " \t,[]", &tok); method; method = stok(NULL, " \t,[]", &tok)) {
        method = strim(method, "'\"", R_TRIM_BOTH);
        if (*method) {
            rAddName(hash, method, "true", 0);
        }
    }
    if (rLookupName(hash, "GET") && !rLookupName(hash, "HEAD")) {
        rAddName(hash, "HEAD", "true", 0);
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
    Parse client-side cache control configuration from route
 */
static void parseCacheControl(WebRoute *route, Json *json, int id)
{
    JsonNode *cacheJson, *ext;
    int      cacheId;

    //  Initialize cache fields
    route->cacheMaxAge = 0;
    route->cacheDirectives = NULL;
    route->extensions = NULL;

    //  Check if cache configuration exists
    cacheJson = jsonGetNode(json, id, "cache");
    if (!cacheJson) {
        return;
    }
    cacheId = jsonGetId(json, id, "cache");
    route->cacheMaxAge = svaluei(jsonGet(json, cacheId, "maxAge", NULL));
    route->cacheDirectives = jsonGet(json, cacheId, "directives", NULL);

    //  Parse extensions array (optional - if not specified, matches all)
    if (jsonGetNode(json, cacheId, "extensions")) {
        route->extensions = rAllocHash(0, 0);
        for (ITERATE_JSON(json, jsonGetNode(json, cacheId, "extensions"), ext, extId)) {
            if (ext->value) {
                rAddName(route->extensions, ext->value, (void*) 1, 0);
            }
        }
    }
}

/*
    Initialize the request routes for the host. Routes match a URL to a request handler and required authenticated role.
 */
/*
    Warn about route keys the server does not understand.

    A route is the only authorization gate in the server, and its keys are the whole of that gate's
    configuration. A typo in "role" or "authType" therefore does not produce a misconfigured route -
    it produces an *unauthenticated* one, silently, because an unrecognised key simply is not read.
    That failure is invisible until someone tests the route from outside, which is the wrong moment
    to discover it.

    Warn rather than refuse: a downstream product may legitimately carry its own keys in the route
    table, and failing to start is a worse outcome than a loud log line. The warning names the route
    so it can be found.
 */
static void checkRouteKeys(cchar *match, Json *json, int id)
{
    static cchar *known[] = {
        "match", "role", "redirect", "trim", "handler", "stream", "validate", "xsrf",
        "compressed", "methods", "cache", "authType", "algorithm", 0
    };
    JsonNode     *child;
    int          i, found;

    for (ITERATE_JSON_KEY(json, id, NULL, child, cid)) {
        if (child->name == 0 || *child->name == '\0') {
            continue;
        }
        for (i = 0, found = 0; known[i]; i++) {
            if (smatch(child->name, known[i])) {
                found = 1;
                break;
            }
        }
        if (!found) {
            rError("web", "Route '%s' has unknown key '%s'. It is ignored. "
                   "Check the spelling - a mistyped 'role' or 'authType' leaves the route unauthenticated.",
                   match && *match ? match : "(catch-all)", child->name);
        }
    }
}

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
        /*
            Fail closed. A host with no route table has not been configured, and the catch-all
            carried no role at all - so a server whose routes failed to load served the whole
            document tree to anonymous clients, silently, with checkRouteKeys never reached because
            there were no routes to check.
         */
        rError("web", "No web.routes configured. Serving the document root to the 'admin' role only. "
               "Declare web.routes, using [{}] for an unauthenticated catch-all.");
        rp = rAllocType(WebRoute);
        rp->match = "";
        rp->handler = "file";
        rp->role = "admin";
        rp->methods = host->methods;
        rp->validate = WEB_VALIDATE_DEFAULT;
        rAddItem(host->routes, rp);

    } else {
        for (ITERATE_JSON(json, routes, route, id)) {
            rp = rAllocType(WebRoute);

            /*
                Slash-ended patterns match prefixes; others match whole segments and subtrees. Empty matches all; /
                matches exactly.
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
            /*
                Absent means "validate whatever the signature file describes". The route key used to
                default to off, so a server configured with web.signatures.enable - the documented
                way to turn validation on - loaded the signature file and validated nothing.
             */
            rp->validate = jsonGet(json, id, "validate", 0) == 0 ? WEB_VALIDATE_DEFAULT :
                           (jsonGetBool(json, id, "validate", 0) ? WEB_VALIDATE_ON : WEB_VALIDATE_OFF);
            rp->xsrf = jsonGetBool(json, id, "xsrf", 0);
            rp->compressed = jsonGetBool(json, id, "compressed", 0);

            //  Parse client-side cache control configuration
            parseCacheControl(rp, json, id);

#if ME_WEB_HTTP_AUTH
            rp->authType = jsonGet(json, id, "authType", 0);
            rp->algorithm = jsonGet(json, id, "algorithm", 0);
            if (rp->algorithm && (!smatch(rp->algorithm, "MD5") && !smatch(rp->algorithm, "SHA-256"))) {
                rError("web",
                       "Route '%s' has unsupported digest algorithm '%s'. Valid: MD5, SHA-256. Ignoring route algorithm.",
                       match ? match : "", rp->algorithm);
                rp->algorithm = 0;
            }
#endif
            if ((methods = jsonToString(json, id, "methods", 0)) != 0) {
                rp->methods = createMethodsHash(methods);
                rFree(methods);
            } else {
                rp->methods = host->methods;
            }
            checkRouteKeys(match, json, id);
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
    Check whether this build supports the auth type; a missing type uses the host default.
 */
static bool supportedAuthType(cchar *authType)
{
    if (!authType) {
        return 1;
    }
#if ME_WEB_AUTH_BASIC
    if (scaselessmatch(authType, "basic")) {
        return 1;
    }
#endif
#if ME_WEB_AUTH_DIGEST
    if (scaselessmatch(authType, "digest")) {
        return 1;
    }
#endif
    return 0;
}

/* Refuse configurations requiring unavailable features; never substitute a weaker authentication scheme. */
static int checkRouteFeatures(WebHost *host)
{
    JsonNode *node;
    cchar    *authType;

#if !ME_WEB_SESSIONS
    WebRoute *route;
    int      next;
#endif

    authType = jsonGet(host->config, 0, "web.auth.authType", 0);
    if (authType && !supportedAuthType(authType)) {
        rError("web", "Unsupported web.auth.authType '%s' in this build", authType);
        return R_ERR_BAD_ARGS;
    }
    //  Read from the configuration: a route keeps no authType without HTTP authentication
    for (ITERATE_JSON_KEY(host->config, 0, "web.routes", node, id)) {
        authType = jsonGet(host->config, id, "authType", 0);
        if (authType && !supportedAuthType(authType)) {
            rError("web", "Route '%s' has unsupported authType '%s' in this build",
                   jsonGet(host->config, id, "match", "(catch-all)"), authType);
            return R_ERR_BAD_ARGS;
        }
    }
#if !ME_WEB_SESSIONS
    for (ITERATE_ITEMS(host->routes, route, next)) {
        if (route->xsrf) {
            rError("web", "Route '%s' requires XSRF tokens, which need sessions, compiled out of this build",
                   route->match && *route->match ? route->match : "(catch-all)");
            return R_ERR_BAD_ARGS;
        }
    }
#endif
    return 0;
}

/*
    Load authentication configuration from web.json5
    Loads users, roles, and HTTP authentication settings
 */
static int loadAuth(WebHost *host)
{
    Json     *json;
    JsonNode *users, *user;
    cchar    *algorithm, *username, *password, *role, *secret;


    json = host->config;

    // Initialize users hash table (always available for session-based auth)
    host->users = rAllocHash(0, 0);

    host->authFailures = rAllocHash(0, R_TEMPORAL_NAME | R_DYNAMIC_VALUE);
    host->maxFailures = svaluei(jsonGet(json, 0, "web.auth.maxFailures", "10"));
    host->failurePeriod = getTimeout(host, "web.auth.failurePeriod", "5mins");

    if (checkRouteFeatures(host) < 0) {
        return R_ERR_BAD_ARGS;
    }

#if ME_WEB_HTTP_AUTH
    // Load HTTP authentication settings (use config strings directly - they persist with host->config)
    host->realm = jsonGet(json, 0, "web.auth.realm", host->name ? host->name : WEB_REALM);
    host->authType = jsonGet(json, 0, "web.auth.authType", "basic");
    algorithm = jsonGet(json, 0, "web.auth.algorithm", "SHA-256");
    if (algorithm && (!smatch(algorithm, "MD5") && !smatch(algorithm, "SHA-256"))) {
        rError("web", "Unsupported digest algorithm '%s'. Valid: MD5, SHA-256. Defaulting to SHA-256.", algorithm);
        host->algorithm = "SHA-256";
    } else {
        host->algorithm = algorithm;
    }
    host->digestTimeout = svaluei(jsonGet(json, 0, "web.timeouts.digest", "60"));
    if (host->digestTimeout <= 0 || host->digestTimeout > 3600) {
        host->digestTimeout = 60;
    }
    host->requireTlsForBasic = jsonGetBool(json, 0, "web.auth.requireTlsForBasic", 1);
    if ((host->opaque = cryptID(32)) == 0) {
        rError("web", "Cannot generate authentication opaque");
        return R_ERR_CANT_INITIALIZE;
    }
    // Generate random secret if not provided
    secret = jsonGet(json, 0, "web.auth.secret", 0);
    if (secret) {
        host->secret = sclone(secret);
    } else {
        // Generate 64-character random alphanumeric ID for HMAC secret
        host->secret = cryptID(64);
    }

    if (!host->secret || !*host->secret) {
        rError("web", "Cannot initialize authentication secret");
        return R_ERR_CANT_INITIALIZE;
    }

#if ME_WEB_AUTH_DIGEST
    // Initialize nonce tracking for replay protection
    // Use R_DYNAMIC_VALUE since we're storing allocated WebNonceEntry pointers, not strings
    host->trackNonces = jsonGetBool(json, 0, "web.auth.track", 1);
    host->nonces = rAllocHash(0, R_TEMPORAL_NAME | R_DYNAMIC_VALUE);
    host->nonceUsers = rAllocHash(0, R_TEMPORAL_NAME | R_STATIC_VALUE);
    if (host->maxDigest <= 0) {
        host->maxDigest = 1000;
    }
    webInitDigestAuth(host);
#endif
#endif

    // Load users from configuration (always available)
    users = jsonGetNode(json, 0, "web.auth.users");
    if (users && users->type == JSON_OBJECT) {
        for (ITERATE_JSON(json, users, user, id)) {
            username = user->name;
            password = jsonGet(json, id, "password", 0);
            role = jsonGet(json, id, "role", "public");
            if (username && password) {
                if (webAddUser(host, username, password, role) == NULL) {
                    rError("web", "Cannot add user %s", username);
                }
            }
        }
    }
#if ME_WEB_HTTP_AUTH
    webInitAuthDummy(host);
#endif
    return 0;
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
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file src/http.c ************/

/*
    http.c - Core HTTP request processing

    Design Notes:
    - This code is a single-threaded server. It does not use multiple threads or processes.
    - It uses fiber coroutines to manage concurrency.
    - It uses non-blocking I/O to manage connections.
    - A connection will block the fiber while servicing the request. Other fibers continue to run if the fiber is
       blocked waiting for I/O.
    - If the connection is idle (keep-alive), the fiber is freed and the connection waits in the event loop for the next
       request.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/


/************************************ Locals **********************************/

#define isWhite(c) ((c) == ' ' || (c) == '\t')

/*
    Limit Content-Length to ssize so ILP32 stores cannot truncate it.
 */
#define WEB_MAX_CONTENT_LENGTH ((int64) MAXSSIZE - 1 < MAXINT64 / 2 ? (int64) MAXSSIZE - 1 : MAXINT64 / 2)

/*
    Valid characters for an HTTP token per RFC 7230. Used for header field names and the request
    method. Allows: A-Z, a-z, 0-9, and special characters: ! # $ % & ' * + - . ^ _ ` | ~
 */
static cchar validHeaderChars[128] = {
    ['A'] = 1, ['B'] = 1, ['C'] = 1, ['D'] = 1, ['E'] = 1, ['F'] = 1, ['G'] = 1,
    ['H'] = 1, ['I'] = 1, ['J'] = 1, ['K'] = 1, ['L'] = 1, ['M'] = 1, ['N'] = 1,
    ['O'] = 1, ['P'] = 1, ['Q'] = 1, ['R'] = 1, ['S'] = 1, ['T'] = 1, ['U'] = 1,
    ['V'] = 1, ['W'] = 1, ['X'] = 1, ['Y'] = 1, ['Z'] = 1,
    ['a'] = 1, ['b'] = 1, ['c'] = 1, ['d'] = 1, ['e'] = 1, ['f'] = 1, ['g'] = 1,
    ['h'] = 1, ['i'] = 1, ['j'] = 1, ['k'] = 1, ['l'] = 1, ['m'] = 1, ['n'] = 1,
    ['o'] = 1, ['p'] = 1, ['q'] = 1, ['r'] = 1, ['s'] = 1, ['t'] = 1, ['u'] = 1,
    ['v'] = 1, ['w'] = 1, ['x'] = 1, ['y'] = 1, ['z'] = 1,
    ['0'] = 1, ['1'] = 1, ['2'] = 1, ['3'] = 1, ['4'] = 1, ['5'] = 1, ['6'] = 1,
    ['7'] = 1, ['8'] = 1, ['9'] = 1,
    ['!'] = 1, ['#'] = 1, ['$'] = 1, ['%'] = 1, ['&'] = 1, ['\''] = 1, ['*'] = 1,
    ['+'] = 1, ['-'] = 1, ['.'] = 1, ['^'] = 1, ['_'] = 1, ['`'] = 1, ['|'] = 1, ['~'] = 1
};


/*
    Default buffer size for rx HTTP headers
 */
#define WEB_HTTP_HEADER_SIZE 1024

/*
    Maximum length of a request method. The longest standard verb is OPTIONS; this leaves room for
    extension verbs while bounding what reaches the Digest HA2 preimage and the trace log.
 */
#define WEB_MAX_METHOD       24

/************************************ Forwards *********************************/

static bool authenticateRequest(Web *web);
static bool authorizeRoute(Web *web, WebRoute *route);
static void logAuthFailure(Web *web, cchar *reason);
static bool authorizeSignature(Web *web);
static bool isValidated(Web *web);
static void freeWebFields(Web *web, bool keepAlive);
static bool acceptHost(Web *web);
static int handleRequest(Web *web);
static bool matchFrom(Web *web, cchar *from);
static bool matchPath(WebHost *host, cchar *path, cchar *pattern, bool exact);
static WebRoute *matchRoute(Web *web, cchar *path);
static int parseHeaders(Web *web, size_t headerSize);
static int parseMethod(Web *web, cchar *method);
static int parseRequestLine(Web *web, char *line);
/*
    Parse an HTTP date header into seconds since the epoch.

    rParseHttpDate returns ticks (milliseconds), matching rGetHttpDate and rParseIsoDate.
    Web's conditional-request fields are time_t seconds, because they are compared against
    stat's st_mtime. Convert here so the conversion lives in one place.
 */
static time_t parseDateHeader(cchar *value)
{
    Time when;

    when = rParseHttpDate(value);
    return when > 0 ? (time_t) (when / TPS) : 0;
}


static bool parseEtags(Web *web, cchar *value);
static bool parseRangeHeader(Web *web, char *header);
static char *parseRangePos(char *str, int64 *value);
static int processBody(Web *web);
static void processOptions(Web *web);
static int processQuery(Web *web);
static int redirectRequest(Web *web);
static void resetWeb(Web *web);
static bool routeRequest(Web *web);
static void setRequestSignature(Web *web, WebRoute *route);
static bool shouldCacheControl(Web *web, WebRoute *route);
static bool credentialedResponse(Web *web);
static int serveRequest(Web *web);
static bool validateRequest(Web *web);
static int webActionHandler(Web *web);
static void webProcessRequest(Web *web);
static void webSetupKeepAliveWait(Web *web);

/************************************* Code ***********************************/
/*
    Allocate a new web connection. This is called by the socket listener when a new connection is accepted.
    This will process the request immediately if data is available.
    webProcessRequest will setup a wait handler if no data is available and will ultimately free the web instance
       object.
 */
PUBLIC int webAlloc(WebListen *listen, RSocket *sock)
{
    Web     *web;
    WebHost *host;

    assert(!rIsMain());

    host = listen->host;

    if (host->connections >= host->maxConnections) {
        /*
            We choose not to generate a 503 response when overloaded like this.
            This is better for DOS protection.
         */
        rTrace("web", "Too many connections %d/%d", (int) host->connections, (int) host->maxConnections);
        rFreeSocket(sock);
        return R_ERR_TOO_MANY;
    }
    if ((web = rAllocType(Web)) == 0) {
        rFreeSocket(sock);
        return R_ERR_MEMORY;
    }
    host->connections++;
    web->conn = ++host->connSequence;
    web->connectionStarted = rGetTicks();
    web->listen = listen;
    web->host = listen->host;
    web->sock = sock;
    web->rx = rAllocBuf(ME_BUFSIZE);
    web->rxHeaders = rAllocBuf(ME_BUFSIZE);
    web->rxRemaining = WEB_UNLIMITED;
    web->txRemaining = WEB_UNLIMITED;
    web->txLen = -1;
    web->rxLen = -1;
    web->signature = -1;
    web->status = 200;
    web->txHeaders = rAllocHash(16, R_DYNAMIC_VALUE);

    rAddItem(host->webs, web);

    if (host->flags & WEB_SHOW_REQ_HEADERS) {
        rLog("raw", "web", "Connect: %s (fd %d)\n", listen->endpoint, sock->fd);
    }
    webHook(web, WEB_HOOK_CONNECT);

    /*
        Try to process immediately - handler will setup wait if no data available
     */
    webProcessRequest(web);
    return 0;
}

/*
    Free the web instance object. This is called when the connection is closing.
    It frees the web instance object fields and the socket.
 */
PUBLIC void webFree(Web *web)
{
    /*
        Freeing the socket can resume a fiber that frees this connection. Prevent nested cleanup from freeing it
        twice.
     */
    if (web->freeing) {
        return;
    }
    web->freeing = 1;
    rRemoveItem(web->host->webs, web);
    rFreeSocket(web->sock);
    rFreeBuf(web->rx);
    freeWebFields(web, 0);
    rFree(web);
}

/*
    Free range request resources
    Used by both freeWebFields and action handlers to clean up ranges
 */
PUBLIC void webFreeRanges(Web *web)
{
    WebRange *range, *next;

    if (web->ranges) {
        for (range = web->ranges; range; range = next) {
            next = range->next;
            rFree(range);
        }
        web->ranges = NULL;
        web->currentRange = NULL;
    }
    rFree(web->rangeBoundary);
    web->rangeBoundary = NULL;
    if (web->mime == web->rmime) {
        web->mime = NULL;
    }
    rFree(web->rmime);
    web->rmime = NULL;
}

/*
    Free the web instance object fields. This is called when the request is complete.
    The web instance object is preserved for the next request if keepAlive is true.
 */
static void freeWebFields(Web *web, bool keepAlive)
{
    /*
        Saved across the memset below when the connection is kept alive, and unused otherwise.
        Initialized at the declaration because the save and the restore sit in separate
        "if (keepAlive)" blocks, which MSVC's flow analysis cannot correlate: it reports every
        one of these as possibly uninitialized at the restore. The values are dead either way.
     */
    RSocket   *sock = NULL;
    RFiber    *fiber = NULL;
    WebListen *listen = NULL;
    Ticks     connectionStarted = 0;
    RBuf      *rx = NULL, *rxHeaders = NULL, *body = NULL, *buffer = NULL;
    RList     *etags = NULL;
    int64     conn = 0, count = 0;
    int       close = 0;

    /*
        If keepAlive is true, we need to save some fields for the next request.
     */
    if (keepAlive) {
        close = web->close;
        conn = web->conn;
        fiber = web->fiber;
        listen = web->listen;
        rx = web->rx;
        sock = web->sock;
        connectionStarted = web->connectionStarted;
        count = web->count;
        rxHeaders = web->rxHeaders;
        body = web->body;
        buffer = web->buffer;
        etags = web->etags;
    }

    //  Free request-specific string resources
    rFree(web->cookie);
    rFree(web->error);
    rFree(web->path);
    rFree(web->redirect);
    rFree(web->securityToken);
    rFree(web->sessionId);
    rFree(web->role);
    web->role = 0;
    rFreeHash(web->txHeaders);

#if ME_WEB_HTTP_AUTH
    rFree(web->authType);
    rFree(web->authDetails);
    rFree(web->username);
    rFree(web->password);
#if ME_WEB_AUTH_DIGEST
    rFree(web->algorithm);
    rFree(web->realm);
    rFree(web->nonce);
    rFree(web->opaque);
    rFree(web->uri);
    rFree(web->qop);
    rFree(web->nc);
    rFree(web->cnonce);
    rFree(web->digestResponse);
    rFree(web->digest);
#endif
#endif
    jsonFree(web->qvars);
    jsonFree(web->vars);
#if ME_WEB_UPLOAD
    webFreeUpload(web);
#endif
    webFreeRanges(web);
    rFree(web->ifMatch);

#if ME_COM_WEBSOCK
    if (web->webSocket) {
        webSocketFree(web->webSocket);
    }
#endif

    /*
        If keepAlive is true, we reuse buffers and lists
     */
    if (keepAlive) {
        rFlushBuf(rxHeaders);
        if (body) {
            rFlushBuf(body);
        }
        if (buffer) {
            rFlushBuf(buffer);
        }
        if (etags) {
            rClearList(etags);
        }
    } else {
        //  Full cleanup - free buffers and list
        rFreeBuf(web->rxHeaders);
        rFreeBuf(web->body);
        rFreeBuf(web->buffer);
        rFreeList(web->etags);
    }

    //  Fast zero of entire structure
    memset(web, 0, sizeof(Web));

    if (keepAlive) {
        //  Restore connection and buffer fields
        web->listen = listen;
        web->rx = rx;
        web->sock = sock;
        web->close = (uint) close;
        web->conn = conn;
        // Reassign on keep-alive entry; one fiber serves all pipelined requests.
        web->fiber = fiber;
        web->connectionStarted = connectionStarted;
        web->count = count;
        web->rxHeaders = rxHeaders;
        web->body = body;
        web->buffer = buffer;
        web->etags = etags;
        //  Recreate txHeaders (simpler than clearing sparse hash)
        web->txHeaders = rAllocHash(16, R_DYNAMIC_VALUE);
    }
}

/**
    Reset the web instance object for the next request. This is called after each request is processed.
    It frees the current request resources and initializes the web instance object for the next request.
 */
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

    //  Set non-zero defaults (buffers already preserved by freeWebFields)
    web->host = web->listen->host;
    web->rxRemaining = WEB_UNLIMITED;
    web->txRemaining = WEB_UNLIMITED;
    web->txLen = -1;
    web->rxLen = -1;
    web->signature = -1;
    web->status = 200;
}

/*
    Signify the connection should be closed when the request completes.
 */
PUBLIC void webClose(Web *web)
{
    if (web) {
        web->close = 1;
    }
}

/*
    Process request(s) on a socket with available data.
    Called directly on connection from webAlloc and as RWait handler
    when socket becomes readable or on timeout.
    May be called multiple times per connection (keep-alive).
    This function blocks during active request processing but returns
    to free the fiber during idle keep-alive periods.

    @param web Web request object
    @param mask I/O event mask (R_READABLE | R_WRITABLE | R_TIMEOUT)
 */
static void webProcessRequest(Web *web)
{
    WebHost *host;
    int     mask;

    host = web->host;
    if (!web->sock || !web->sock->wait) {
        return;
    }
    //  Explicit timeout detection - critical for resource cleanup
    mask = web->sock->wait->eventMask;
    int blockResult = 0;
    if (mask & R_TIMEOUT) {
        rTrace("web", "Keep-alive inactivity timeout on connection %lld", web->conn);
        web->close = 1;
    } else {
#if ME_WEB_FIBER_BLOCKS
        /*
            Catch exceptions during request processing using setjmp/longjmp.
            Enable via web.fiberBlocks configuration property.
            Uses short-circuit evaluation: if fiberBlocks is false, rStartFiberBlock is never called.
         */

        if (host->fiberBlocks) {
            rStartFiberBlock();
            blockResult = setjmp(rGetFiber()->jmpbuf);
        }
        if (!host->fiberBlocks || blockResult == 0) {
#endif
        web->fiber = rGetFiber();
        /* Disarm the idle wait while this fiber owns the request, including waits elsewhere. */
        rSetWaitHandler(web->sock->wait, NULL, NULL, 0, 0, 0);

        while (!web->close) {
            //  Process one complete request (blocks for I/O as needed)
            if (serveRequest(web) < 0) {
                break;
            }
            //  Check if we should continue
            if (web->close || web->sock->fd == INVALID_SOCKET) {
                break;
            }
            //  Reset web instance object for next request
            resetWeb(web);
            if (web->close) {

                break;
            }
            if (rGetBufLength(web->rx) == 0) {
                //  No buffered data, setup wait for next request
                webSetupKeepAliveWait(web);
                return;
            }
            //  Continue loop to process pipelined requests
        }
#if ME_WEB_FIBER_BLOCKS
    } else {
        /*
            This is a best effort to allow the server to continue serving other requests. The user should cleanup
               resources via the HOOK.
         */
        rEndFiberBlock();
        webHook(web, WEB_HOOK_EXCEPTION);
        rError("web", "Exception in handler processing for %s\n", web->path);
        web->close = 1;
    }
#endif
    }
    if (host->flags & WEB_SHOW_REQ_HEADERS) {
        rLog("raw", "web", "Disconnect: %s (fd %d)\n", web->listen->endpoint, web->sock->fd);
    }
    webHook(web, WEB_HOOK_DISCONNECT);
    webFree(web);
    host->connections--;
}

/*
    Setup wait for next keep-alive request.
    Configures RWait to call webProcessRequest when data arrives or timeout expires.
 */
static void webSetupKeepAliveWait(Web *web)
{
    Ticks deadline;

    //  Calculate inactivity timeout deadline
    if (rGetTimeouts()) {
        deadline = rGetTicks() + web->host->inactivityTimeout;
    } else {
        deadline = 0;
    }
    //  Setup wait handler
    rSetWaitHandler(web->sock->wait, (RWaitProc) webProcessRequest, web, R_READABLE, deadline, 0);
}

/*
    Serve a request. This routine blocks the current fiber while waiting for I/O.
 */
static int serveRequest(Web *web)
{
    char *cp;
    ssize size;
    size_t len;

    web->started = rGetTicks();

    /*
        The parse budget applies to every request on the connection, not only the first.
        serveRequest is entered because the socket became readable, so the request is already
        arriving and the header block should complete within parseTimeout whatever the request
        count. Giving a keep-alive request the inactivity timeout instead meant a peer could hold
        the fiber - and so one of the pool's connection slots - for minutes by sending a partial
        request line on a connection it had already used once, which is cheaper for an attacker
        than opening a new one.
     */
    web->deadline = rGetTimeouts() ? web->started + web->host->parseTimeout : 0;

    //  Before parsing, so errors answered during parsing carry the security headers too
    webAddStandardHeaders(web);

    /*
        Read until we have all the headers up to the limit
     */
    if ((size = webBufferUntil(web, "\r\n\r\n", (size_t) web->host->maxHeader)) <= 0) {
        if (rGetBufLength(web->rx) >= (size_t) web->host->maxHeader) {
            if (web->host->flags & WEB_SHOW_REQ_HEADERS) {
                if ((cp = strchr(web->rx->start, '\n')) != 0) {
                    len = (size_t) (cp - web->rx->start);
                } else {
                    len = (size_t) rGetBufLength(web->rx);
                }
                len = min(len, 80);
                rLog("raw", "web", "Request <<<<\n\n%.*s\n", (int) len, web->rx->start);
            }
            return webError(web, -413, "Request headers too big");
        }
        // I/O error, timeout, or pattern not found before limit. A timeout has already answered.
        return R_ERR_CANT_READ;
    }
    web->count++;
    web->headerSize = size;

    if (parseHeaders(web, (size_t) size) < 0) {
        return R_ERR_BAD_REQUEST;
    }
    webHook(web, WEB_HOOK_START);

    if (handleRequest(web) < 0) {
        return R_ERR_CANT_COMPLETE;
    }
    webHook(web, WEB_HOOK_END);
    return 0;
}

/* Check web.hosts against Host to prevent DNS rebinding. Entries match the whole value or hostname without port. */
static bool acceptHost(Web *web)
{
    JsonNode *child;
    cchar *hostHeader, *end;
    size_t nameLen;

    if (web->host->hosts < 0) {
        return 1;
    }
    if ((hostHeader = webGetHeader(web, "host")) == 0) {
        //  HTTP/1.0 without Host: the request names no authority to misdirect
        return 1;
    }
    // Strip the port without splitting bracketed IPv6 literals.
    if (*hostHeader == '[') {
        end = schr(hostHeader, ']');
        end = end ? end + 1 : hostHeader + slen(hostHeader);
    } else {
        end = schr(hostHeader, ':');
        end = end ? end : hostHeader + slen(hostHeader);
    }
    nameLen = (size_t) (end - hostHeader);

    for (ITERATE_JSON_ID(web->host->config, web->host->hosts, child, id)) {
        if (scaselessmatch(child->value, hostHeader) ||
            (slen(child->value) == nameLen && sncaselesscmp(child->value, hostHeader, nameLen) == 0)) {
            return 1;
        }
    }
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
    cchar *handler;

    if (web->error) {
        return 0;
    }
    if (!acceptHost(web)) {
        webError(web, 421, "Misdirected request");
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
#if ME_WEB_UPLOAD
    if (web->uploads && webProcessUpload(web) < 0) {
        return 0;
    }
#endif
    if (web->query && processQuery(web) < 0) {
        //  An error response has been generated
        return 0;
    }
#if ME_COM_WEBSOCK
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
    if (web->route->xsrf || (web->policy && web->policy->xsrf)) {
        if (web->get) {
            /*
                This will generate a new XSRF token if there is not one already in the session state.
             */
            if (webAddSecurityToken(web, 0) < 0) {
                return R_ERR_CANT_COMPLETE;
            }
        } else if (!web->options && !web->head && !web->trace) {
            if (!webCheckSecurityToken(web)) {
                if (!web->error) {
                    webError(web, 400, "Invalid XSRF token");
                }
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
    assert(web);

    if (web->host->signatures && isValidated(web)) {
        return webValidateRequest(web);
    }
    return 1;
}


static bool isValidated(Web *web)
{
    return web->route->validate != WEB_VALIDATE_OFF || (web->policy && web->policy->validate != WEB_VALIDATE_OFF);
}

/*
    Select the signature that describes this request, by slicing the route prefix off the path and
    turning the separators into property terms.

    The key must come from the same string routing and dispatch use. It was sliced off web->url,
    which parseUrl decodes but does not normalize, while every other stage uses the normalized
    web->path. A ".." segment therefore keyed a different signature, or none - and a missing
    signature fails open unless web.signatures.strict is set, which is not the default. Deriving it
    here, from web->path before any trim rewrite, makes route->match a prefix of the string being
    sliced by construction, so the offset is well defined for every spelling.
 */
static void setRequestSignature(Web *web, WebRoute *route)
{
    char *key, *cp;
    size_t len;
    int sid;

    if (!web->host->signatures) {
        return;
    }
    len = slen(route->match);
    if (slen(web->path) < len) {
        return;
    }
    /*
        Skip separators so the key never starts with an empty term: an empty or leading-empty key
        resolves to the root of the signature file rather than to no signature at all.
     */
    for (cp = &web->path[len]; *cp == '/'; cp++) {
    }
    if (*cp == '\0') {
        //  Nothing beyond the route prefix names no endpoint
        return;
    }
    key = sclone(cp);
    for (cp = key; *cp; cp++) {
        if (*cp == '/') {
            *cp = '.';
        }
    }
    /*
        Choose the longest endpoint prefix with a request/response block so trailing segments cannot bypass its
        signature.
     */
    for (;;) {
        sid = jsonGetId(web->host->signatures, 0, key);
        if (sid >= 0 && (jsonGetId(web->host->signatures, sid, "request") >= 0 ||
                         jsonGetId(web->host->signatures, sid, "response") >= 0)) {
            web->signature = sid;
            break;
        }
        if ((cp = srchr(key, '.')) == 0) {
            break;
        }
        *cp = '\0';
    }
    rFree(key);
}

/*
    Test if a path matches an action prefix. The prefix must cover whole path segments, so
    "/api/status" does not answer for "/api/statusX", and a prefix that already ends in "/" is a
    boundary in itself.
 */
static bool matchAction(cchar *path, cchar *match)
{
    size_t len;

    if (!sstarts(path, match)) {
        return 0;
    }
    len = slen(match);
    return len == 0 || match[len - 1] == '/' || path[len] == '\0' || path[len] == '/';
}

/*
    Handle an action request. This is called when the request is a valid action request.
    It will run the action function and return the result.
 */
static int webActionHandler(Web *web)
{
    WebAction *action, *best;
    size_t bestLen, len;
    int next;

    /*
        Select the longest matching prefix. A first-match dispatch would let an action registered
        at a short prefix answer for a longer, later registration - including one that carries a
        stronger role - so the shorter registration must not shadow the longer one.
     */
    best = 0;
    bestLen = 0;
    for (ITERATE_ITEMS(web->host->actions, action, next)) {
        if (matchAction(web->path, action->match)) {
            len = slen(action->match);
            if (!best || len > bestLen) {
                best = action;
                bestLen = len;
            }
        }
    }
    if (!best) {
        return webError(web, 404, "No action to handle request");
    }
    /*
        For public actions (role == NULL or "public"), do not deny access.
        Attempt authorization only if a specific non-public role is required.
     */
    if (best->role && !smatch(best->role, "public")) {
        if (!webCan(web, best->role)) {
            webError(web, 403, "Access Denied. User has insufficient privilege.");
            return 0;
        }
    }
    /*
        Ignore range requests for dynamic content
        Action handlers generate dynamic content that cannot be ranged
     */
    webFreeRanges(web);
    //  Set Accept-Ranges: none for dynamic content
    webAddHeaderStaticString(web, "Accept-Ranges", "none");

    webHook(web, WEB_HOOK_ACTION);
    (best->fn)(web);
    return 0;
}

/*
    Route the request. This matches the request URL with route URL prefixes.
    It also authorizes the request by checking the authenticated user role vs the routes required role.
    Return true if the request was routed successfully.
 */
static bool routeRequest(Web *web)
{
    WebRoute *route, *target;
    char *path;
    size_t trimLen;

    if ((route = matchRoute(web, web->path)) == 0) {
        rInfo("web", "Cannot find route to serve request %s", web->path);
        webHook(web, WEB_HOOK_NOT_FOUND);

        if (!web->error) {
            webWriteResponseString(web, 404, "No matching route");
        }
        return 0;
    }
    if (!rLookupName(route->methods, web->method)) {
        webError(web, 405, "Unsupported method.");
        return 0;
    }
    web->route = route;
    if (!authorizeRoute(web, route)) {
        return 0;
    }
    /*
        A redirect is a complete response, not a route attribute applied alongside handling. Return
        zero so dispatch stops here: the handler would otherwise still run and putFile, deleteFile
        and action functions would perform their side effects behind the 302. This matches how
        redirectRequest terminates a host-level redirect.
     */
    if (route->redirect) {
        webRedirect(web, 302, route->redirect);
        return 0;
    }
    //  Before any trim rewrite, while route->match is still a prefix of web->path
    setRequestSignature(web, route);

    if (route->trim && matchPath(web->host, web->path, route->trim, 0)) {
        trimLen = slen(route->trim);
        path = sclone(&web->path[trimLen]);
        //  The extension borrows storage from the path being replaced.
        if (web->ext) {
            web->ext = web->ext >= web->path + trimLen ? path + (web->ext - (web->path + trimLen)) : 0;
        }
        rFree(web->path);
        web->path = path;
        web->trimmed = 1;
        /*
            Keep the trim route for dispatch, but enforce the destination route's role, unsafe methods, XSRF and
            validation too. Safe methods remain governed by the trim route.
         */
        if ((target = matchRoute(web, web->path)) != 0 && target != route) {
            if (!rLookupName(target->methods, web->method) &&
                !(web->get || web->head || web->options || web->trace)) {
                webError(web, 405, "Unsupported method.");
                return 0;
            }
            if (!authorizeRoute(web, target)) {
                return 0;
            }
            web->policy = target;

            web->signature = -1;
            setRequestSignature(web, target);
        }
    }
    if (!authorizeSignature(web)) {
        return 0;
    }
    return 1;
}

/* Require rewritten destinations to authorize the request too; retain the matched route for dispatch and policy. */
PUBLIC bool webAuthorizePath(Web *web, cchar *path)
{
    WebRoute *target;

    if ((target = matchRoute(web, path)) != 0 && target != web->route && !authorizeRoute(web, target)) {
        return 0;
    }
    return 1;
}

/*
    Enforce the endpoint role or controller _meta.role; field filtering alone does not authorize access.
 */
static bool authorizeSignature(Web *web)
{
    Json *signatures;
    JsonNode *controller;
    cchar *role;

    signatures = web->host->signatures;
    if (!signatures || web->signature < 0 || !isValidated(web) || web->options) {
        return 1;
    }
    if ((role = jsonGet(signatures, web->signature, "role", 0)) == 0) {
        for (ITERATE_JSON(signatures, NULL, controller, cid)) {
            if (cid < web->signature && web->signature < controller->last) {
                role = jsonGet(signatures, cid, "_meta.role", 0);
                break;
            }
        }
    }
    if (!role || smatch(role, "public") || webCan(web, role)) {
        return 1;
    }
    if (web->authenticated) {
        webError(web, 403, "Access Denied. User has insufficient privilege.");
    } else {
        webError(web, 401, "Authentication required");
    }
    return 0;
}

/*
    Compare a request path against a route pattern, folding case when the documents filesystem does.

    Route selection is the only authorization gate in the server. A byte-exact compare against a
    filesystem that resolves many spellings to one file decides authorization on a different string
    from the one open() resolves, so an upper-case spelling of a protected prefix misses the route
    that guards it and falls through to a later route carrying no role. webAllocHost probes the
    documents filesystem once and sets host->foldCase from what it finds.
 */
static bool matchPath(WebHost *host, cchar *path, cchar *pattern, bool exact)
{
    size_t len;

    len = slen(pattern);
    if (!(host->foldCase ? sncaselesscmp(path, pattern, len) == 0 : sstarts(path, pattern))) {
        return 0;
    }
    if (exact) {
        //  A pattern without a trailing "/" names a resource and its subtree, on a segment boundary
        return path[len] == '\0' || (path[len] == '/' && len > 1);
    }
    return 1;
}

/*
    Find the first route whose match pattern covers the path. Returns NULL if none.
 */
static WebRoute *matchRoute(Web *web, cchar *path)
{
    WebRoute *route;
    int next;

    for (ITERATE_ITEMS(web->host->routes, route, next)) {
        if (matchPath(web->host, path, route->match, route->exact)) {
            return route;
        }
    }
    return 0;
}

/*
    Apply a route's authentication and authorization gate. The route is bound to web->route for the
    duration so the challenge realm and digest algorithm come from the route being enforced.
    Return true if the request may proceed.
 */
static bool authorizeRoute(Web *web, WebRoute *route)
{
    WebRoute *prior;
    bool rc;

    if (!route->role || smatch(route->role, "public") || web->options) {
        return 1;
    }
    prior = web->route;
    web->route = route;
    rc = 1;
    if (!authenticateRequest(web)) {
        logAuthFailure(web, web->status == 429 ? "too many failures" : "authentication failed");
        /*
            HTTP authentication has already written its challenge. Session authentication has
            nothing to challenge with, so answer rather than leave the request hanging until the
            connection times out.
         */
        if (!web->error) {
            webError(web, 401, "Authentication required");
        }
        rc = 0;

    } else if (!webCan(web, route->role)) {
        logAuthFailure(web, "insufficient privilege");
        webError(web, 403, "Access Denied. User has insufficient privilege.");
        rc = 0;
    }
    web->route = prior;
    return rc;
}

/*
    Audit route refusals only when credentials or cookies were presented, so an ordinary first challenge is not logged.
 */
static void logAuthFailure(Web *web, cchar *reason)
{
    bool presented;

    presented = web->cookie != 0;
#if ME_WEB_HTTP_AUTH
    presented = presented || web->authDetails;
#endif
    if (presented) {
        webLogAuthFailure(web, reason);
    }
}

/*
    Never log credentials or client-supplied usernames.
 */
PUBLIC void webLogAuthFailure(Web *web, cchar *reason)
{
    cchar *match;
    char  ip[64];
    int   port;

    if (!web) {
        return;
    }
    if (rGetSocketPeer(web->sock, ip, sizeof(ip), &port) < 0 || !*ip) {
        scopy(ip, sizeof(ip), "unknown");
    }
    match = web->route && web->route->match && *web->route->match ? web->route->match : "(catch-all)";
    rInfo("auth", "Refused %s from %s on connection %lld: %s (route '%s')", web->method, ip, web->conn,
          reason ? reason : "refused", match);
}

static bool authenticateRequest(Web *web)
{
    WebRoute *route;

    route = web->route;
#if ME_WEB_HTTP_AUTH
    if (route->authType && !smatch(route->role, "public")) {
        /*
            If route specifies an auth type, enforce HTTP authentication.
            Public routes should never deny access due to auth.
         */
        return webHttpAuthenticate(web);
    }
#endif
    //  Otherwise, allow session-based authentication. Logged in users get session setup even for public routes
    if (webAuthenticate(web)) {
        return 1;
    }
    return smatch(route->role, "public");
}

/*
    Apply top level redirections. This is used to redirect to https and site redirections.
 */
static int redirectRequest(Web *web)
{
    WebRedirect *redirect;
    int next;

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
    char *buf, ip[256];
    cchar *host, *path, *query, *hash, *scheme;
    bool rc;
    int port, portNum;

    if ((buf = webParseUrl(from, &scheme, &host, &port, &path, &query, &hash)) == 0) {
        webWriteResponseString(web, 404, "Cannot parse redirection target");
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
    Parse ETag list from If-Match, If-None-Match headers
    Formats: "etag1", "etag1", "etag2", or *
    Returns true if valid, false if malformed
 */
static bool parseEtags(Web *web, cchar *value)
{
    char *copy, *tok;

    //  Reuse existing list if preserved from keep-alive (already cleared), otherwise allocate
    if (!web->etags) {
        web->etags = rAllocList(0, R_TEMPORAL_VALUE);
    }

    //  Check for wildcard
    if (smatch(value, "*")) {
        rAddItem(web->etags, "*");
        return 1;
    }

    //  Parse comma-separated ETags
    copy = sclone(value);

    for (tok = stok(copy, ",", (char**) &value); tok; tok = stok(NULL, ",", (char**) &value)) {
        tok = strim(tok, " \t", R_TRIM_BOTH);

        //  ETags must be quoted strings - strip quotes for faster comparison
        if (*tok == '"') {
            rAddItem(web->etags, strim(tok, "\"", R_TRIM_BOTH));
        } else if (*tok == 'W' && tok[1] == '/' && tok[2] == '"') {
            //  Weak ETags: W/"etag" - strip W/ prefix and quotes
            rAddItem(web->etags, strim(tok + 2, "\"", R_TRIM_BOTH));
        } else {
            //  Malformed ETag - clear but keep list for reuse
            rClearList(web->etags);
            rFree(copy);
            return 0;
        }
    }
    rFree(copy);
    return rGetListLength(web->etags) > 0;
}

/*
    Parse Range header value like "bytes=0-499" or "bytes=0-49,100-149"
    Returns true if valid, false if malformed

    Supported formats:
    - "bytes=0-499"        First 500 bytes
    - "bytes=500-999"      Bytes 500-999
    - "bytes=-500"         Last 500 bytes
    - "bytes=500-"         From byte 500 to end
    - "bytes=0-49,100-149" Multiple ranges
 */
/*
    Parse decimal byte positions, saturating at MAXINT64-1 to leave room for exclusive ends. Return the next
    character, or NULL without digits.
 */
static char *parseRangePos(char *str, int64 *value)
{
    int64 limit, v;
    int digit;

    if (!isdigit((uchar) * str)) {
        return 0;
    }
    limit = MAXINT64 - 1;
    for (v = 0; isdigit((uchar) * str); str++) {
        digit = *str - '0';
        v = (v > (limit - digit) / 10) ? limit : v * 10 + digit;
    }
    *value = v;
    return str;
}

static bool parseRangeHeader(Web *web, char *header)
{
    WebRange *range, *last;
    int64 end, len, start;
    char *tok, *ep, *value;
    int count;

    //  Must start with "bytes="
    if (!sstarts(header, "bytes=")) {
        return 0;
    }
    header += 6;  // Skip "bytes="
    last = NULL;
    count = 0;

    //  Parse comma-separated ranges
    for (tok = stok(header, ",", (char**) &value); tok; tok = stok(NULL, ",", (char**) &value)) {
        tok = strim(tok, " \t", R_TRIM_BOTH);
        len = 0;

        //  Parse start-end. Each position must be followed by exactly the expected delimiter.
        if (*tok == '-') {
            //  Suffix range: -500 means last 500 bytes
            if ((ep = parseRangePos(tok + 1, &end)) == 0 || *ep || end <= 0) {
                return 0;
            }
            start = -1;
        } else {
            if ((ep = parseRangePos(tok, &start)) == 0 || *ep != '-') {
                return 0;
            }
            if (ep[1] == '\0') {
                //  Open-ended: 500- means from 500 to end
                end = -1;
            } else {
                //  Normal range: 0-499
                if ((ep = parseRangePos(ep + 1, &end)) == 0 || *ep) {
                    return 0;
                }
                end++;  // Exclusive end. Cannot overflow: parseRangePos saturates below MAXINT64.
            }
        }
        //  Validate range
        if (start >= 0 && end >= 0) {
            if (start >= end) {
                return 0;
            }
            len = end - start;
        }
        /*
            Each range is served in full, so a header listing the same span hundreds of times
            turns a small request into a response hundreds of times the file. RFC 9110 14.2 lets a
            server ignore a Range header it chooses not to honour: past the cap the whole
            document is served as a plain 200, which is bounded by the file size.
         */
        if (++count > WEB_MAX_RANGES) {
            rTrace("web", "Ignoring Range header with more than %d ranges", WEB_MAX_RANGES);
            webFreeRanges(web);
            return 1;
        }
        range = rAllocType(WebRange);
        range->start = start;
        range->end = end;
        range->len = len;
        //  Add to linked list
        if (last == NULL) {
            web->ranges = range;
        } else {
            last->next = range;
        }
        last = range;
    }
    web->currentRange = web->ranges;
    return web->ranges != NULL;
}

/*
    Check if currentEtag matches any ETag in If-Match or If-None-Match list
    Returns true if there's a match
    Handles wildcard (*) matching
 */
PUBLIC bool webMatchEtag(Web *web, cchar *currentEtag)
{
    char *etag;
    int i, len;

    if (!web->etags || !currentEtag) {
        return 0;
    }
    len = rGetListLength(web->etags);
    for (i = 0; i < len; i++) {
        etag = rGetItem(web->etags, i);
        //  Wildcard matches any ETag
        if (smatch(etag, "*")) {
            return 1;
        }
        //  Direct match (both strong and weak ETags)
        if (smatch(etag, currentEtag)) {
            return 1;
        }
    }
    return 0;
}

/*
    Check if resource was modified based on If-Modified-Since or If-Unmodified-Since
    Returns true if the condition evaluates to true per RFC 7232
 */
PUBLIC bool webMatchModified(Web *web, time_t mtime)
{
    //  If-Modified-Since: true if resource was modified after the given time
    if (web->ifModified && web->since > 0) {
        return mtime > web->since;
    }
    //  If-Unmodified-Since: true if resource was not modified after the given time
    if (web->ifUnmodified && web->unmodifiedSince > 0) {
        return mtime <= web->unmodifiedSince;
    }
    //  No conditional headers present
    return 1;
}

/*
    Determine if 304 Not Modified should be returned
    Per RFC 7232 section 6:
    - If-None-Match takes precedence over If-Modified-Since
    - Used for GET and HEAD requests only
    Returns true if content is not modified
 */
PUBLIC bool webContentNotModified(Web *web, cchar *currentEtag, time_t mtime)
{
    //  Only applicable to GET and HEAD requests
    if (!web->get && !web->head) {
        return 0;
    }
    /*
        If-None-Match has priority over If-Modified-Since
        Per RFC 7232 section 3.2
     */
    if (web->ifNoneMatch && web->etags) {
        //  If ETag matches, content not modified
        return webMatchEtag(web, currentEtag);
    }
    //  Fall back to If-Modified-Since
    if (web->ifModified && web->since > 0) {
        //  If not modified since the given time, content not modified
        return mtime <= web->since;
    }
    //  No conditional headers, assume modified
    return 0;
}

/*
    Parse the request headers
 */
static int parseHeaders(Web *web, size_t headerSize)
{
    RBuf *buf;
    char *end, *line, *redacted, *tok;

    buf = web->rx;
    if (headerSize <= 10 || headerSize > (size_t) rGetBufLength(buf)) {
        return webNetError(web, "Bad request header");
    }
    end = buf->start + headerSize;
    end[-2] = '\0';

    /*
        Reserve room for the block and its terminator together. rPutBlockToBuf skips its trailing
        NUL when it fills the buffer exactly, and the deferred rAddNullToBuf would then grow the
        buffer - reallocating and freeing the storage that web->method, web->url and web->protocol
        point into, since the request line is tokenised in place below. Reserving here keeps those
        pointers valid and keeps the block terminated for the string operations that follow.
     */
    if (rReserveBufSpace(web->rxHeaders, headerSize - 1) < 0) {
        return webNetError(web, "Cannot allocate request header buffer");
    }
    rPutBlockToBuf(web->rxHeaders, buf->start, headerSize - 2);
    rAdjustBufStart(buf, end - buf->start);

    if (web->host->flags & WEB_SHOW_REQ_HEADERS) {
        redacted = webRedactHeaders(web->rxHeaders->start);
        rLog("raw", "web", "Request <<<<\n\n%s\n", redacted);
        rFree(redacted);
    }
    line = web->rxHeaders->start;
    if ((tok = scontains(line, "\r\n")) == 0) {
        return webNetError(web, "Bad request header");
    }
    *tok = '\0';
    if (parseRequestLine(web, line) < 0) {
        return R_ERR_BAD_REQUEST;
    }
    web->scheme = rIsSocketSecure(web->sock) ? "https" : "http";

    rAdjustBufStart(web->rxHeaders, tok + 2 - line);
    rAddNullToBuf(web->rxHeaders);

    if (!webParseHeadersBlock(web, web->rxHeaders->start, rGetBufLength(web->rxHeaders), 0)) {
        return R_ERR_BAD_REQUEST;
    }
    if (webValidateUrl(web) < 0) {
        return R_ERR_BAD_REQUEST;
    }
    webUpdateDeadline(web);
    return 0;
}

/*
    Parse the request line: method SP request-target SP HTTP-version. The line has already been
    terminated at its CRLF.

    RFC 9112 section 3 permits exactly one space at each delimiter and exactly three tokens. Runs of
    spaces, tabs as delimiters, a trailing token or an unknown version are all shapes a front end may
    resolve differently to this server, which is a request-smuggling differential, so each is refused
    here rather than normalized. Validating the version also makes web->protocol one of two server
    owned constants, so the response status line can never carry client supplied bytes.
 */
static int parseRequestLine(Web *web, char *line)
{
    char *protocol, *url;

    if ((url = strchr(line, ' ')) == 0 || url[1] == ' ') {
        return webError(web, -400, "Bad request line");
    }
    *url++ = '\0';
    if ((protocol = strchr(url, ' ')) == 0 || protocol[1] == ' ') {
        return webError(web, -400, "Bad request line");
    }
    *protocol++ = '\0';
    if (*url == '\0' || *protocol == '\0' || strchr(protocol, ' ') != 0) {
        //  Empty or absent token, or a fourth token trailing the version
        return webError(web, -400, "Bad request line");
    }
    if (smatch(protocol, "HTTP/1.1")) {
        web->protocol = "HTTP/1.1";
    } else if (smatch(protocol, "HTTP/1.0")) {
        /*
            Only support HTTP/1.0 without keep alive - all clients should be supporting HTTP/1.1
         */
        web->protocol = "HTTP/1.0";
        web->http10 = 1;
        web->close = 1;
    } else {
        return webError(web, -505, "Unsupported HTTP version");
    }
    //  RFC 9110 9.1: the method is case-sensitive, so it is not normalized
    if (parseMethod(web, line) < 0) {
        return R_ERR_BAD_REQUEST;
    }
    web->url = url;
    return 0;
}

/*
    Validate and record the request method. The route's method hash rejects unknown verbs, but the
    method also reaches the Digest HA2 preimage and the trace log, so bound it to an RFC 9110 token
    of sane length here.
 */
static int parseMethod(Web *web, cchar *method)
{
    cchar *cp;
    uchar uc;

    if (!method || *method == '\0' || slen(method) > WEB_MAX_METHOD) {
        return webError(web, -400, "Bad request method");
    }
    for (cp = method; *cp; cp++) {
        uc = (uchar) * cp;
        if (uc >= sizeof(validHeaderChars) || !validHeaderChars[uc]) {
            return webError(web, -400, "Bad request method");
        }
    }
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
    web->method = method;
    return 0;
}

/* Parse Connection as comma-separated options with optional whitespace (RFC 9110 7.6.1). */
static void parseConnection(Web *web, cchar *value)
{
    cchar *cp, *start;
    size_t len;

    for (cp = value; *cp; ) {
        while (*cp == ' ' || *cp == '\t' || *cp == ',') {
            cp++;
        }
        for (start = cp; *cp && *cp != ','; cp++) {
        }
        len = (size_t) (cp - start);
        while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '\t')) {
            len--;
        }
        if (len == 5 && sncaselesscmp(start, "close", 5) == 0) {
            web->close = 1;
        } else if (len == 7 && sncaselesscmp(start, "upgrade", 7) == 0) {
            web->connUpgrade = 1;
        }
    }
}

/*
    Parse a headers block. Used here and by file upload.
 */
PUBLIC bool webParseHeadersBlock(Web *web, char *headers, size_t headersSize, bool upload)
{
    cchar *end;
    char c, *cp, *endKey, *idEnd, *key, *prior, *t, *value;
    uchar uc;
    int64 eventId, length;
    bool hasAuth = 0, hasCL = 0, hasCT = 0, hasHost = 0, hasIfRange = 0, hasRange = 0, hasTE = 0;

    if (headers && *headers) {
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

            /*
                Trim trailing whitespace before the replaced CR/LF terminators. Iterators skip the resulting NUL
                run.
             */
            for (t = cp - 3; t >= value && isWhite(*t); t--) {
                *t = '\0';
            }
            //  RFC 9110 5.1: a field name is at least one token character
            if (key == endKey) {
                webError(web, -400, "Empty header name");
                return 0;
            }

            //  Validate header name
            for (t = key; t < endKey; t++) {
                uc = (uchar) * t;
                if (uc >= sizeof(validHeaderChars) || !validHeaderChars[uc]) {
                    return 0;
                }
            }
            c = (char) tolower((uchar) * key);

            if (upload && c != 'c' && (
                    !scaselessmatch(key, "content-disposition") &&
                    !scaselessmatch(key, "content-type"))) {
                webNetError(web, "Bad upload headers");
                return 0;
            }
            if (c == 'a' && scaselessmatch(key, "authorization")) {
                if (hasAuth) {
                    webError(web, -400, "Duplicate Authorization");
                    return 0;
                }
                hasAuth = 1;
#if ME_WEB_HTTP_AUTH
                //  Parse Authorization header: "Basic xxx" or "Digest xxx"
                char *authType = value;
                char *sp = strchr(value, ' ');
                if (sp) {
                    *sp++ = '\0';
                    web->authType = sclone(authType);
                    web->authDetails = sclone(sp);
                }
#endif
            } else
            if (c == 'c') {
                if (scaselessmatch(key, "content-disposition")) {
                    web->contentDisposition = value;

                } else if (scaselessmatch(key, "content-type")) {
                    /*
                        Reject repeats to avoid reinitializing upload state.
                     */
                    if (hasCT) {
                        webError(web, -400, "Duplicate Content-Type header");
                        return 0;
                    }
                    hasCT = 1;
                    web->contentType = value;
                    if (scontains(value, "multipart/form-data")) {
#if ME_WEB_UPLOAD
                        if (webInitUpload(web) < 0) {
                            return 0;
                        }
#else
                        //  Refuse rather than parse a multipart body as something else
                        webError(web, 415, "Uploads are not supported");
                        return 0;
#endif

                    } else if (smatch(value, "application/x-www-form-urlencoded")) {
                        web->formBody = 1;

                    } else if (smatch(value, "application/json")) {
                        web->jsonBody = 1;
                    }

                } else if (scaselessmatch(key, "connection")) {
                    parseConnection(web, value);
                } else if (scaselessmatch(key, "content-length")) {
                    /*
                        RFC 9112 6.3: the value is 1*DIGIT and nothing else. A sign, a suffix, a
                        comma list or a second differing Content-Length is a request a front-end
                        may frame differently from us, which is a smuggling primitive, so each is
                        refused. Repeated identical values are tolerated as the RFC permits.
                     */
                    if (sparseUint(value, 10, WEB_MAX_CONTENT_LENGTH, &t, &length) < 0 || *t != '\0') {
                        webError(web, -400, "Bad Content-Length");
                        return 0;
                    }
                    if (hasCL && length != web->rxLen) {
                        webError(web, -400, "Conflicting Content-Length");
                        return 0;
                    }
                    hasCL = 1;
                    web->rxLen = web->rxRemaining = (ssize) length;

                } else if (scaselessmatch(key, "cookie")) {
                    if (web->cookie) {
                        prior = web->cookie;
                        web->cookie = sjoin(prior, "; ", value, NULL);
                        rFree(prior);
                    } else {
                        web->cookie = sclone(value);
                    }
                }

            } else if (c == 'i') {
                if (scaselessmatch(key, "if-match")) {
                    if (!parseEtags(web, value)) {
                        webError(web, 400, "Invalid If-Match header");
                        return 0;
                    }
                    web->ifMatchPresent = 1;

                } else if (scaselessmatch(key, "if-modified-since")) {
                    web->since = parseDateHeader(value);
                    if (web->since > 0) {
                        web->ifModified = 1;
                    }

                } else if (scaselessmatch(key, "if-none-match")) {
                    if (!parseEtags(web, value)) {
                        webError(web, 400, "Invalid If-None-Match header");
                        return 0;
                    }
                    web->ifNoneMatch = 1;

                } else if (scaselessmatch(key, "if-range")) {
                    /*
                        One If-Range only. A repeat overwrote web->ifMatch without freeing the
                        previous clone, leaking on every repeat.
                     */
                    if (hasIfRange) {
                        webError(web, -400, "Duplicate If-Range header");
                        return 0;
                    }
                    hasIfRange = 1;
                    //  Can be either an ETag or a date - strip quotes for faster comparison
                    if (*value == '"') {
                        web->ifMatch = sclone(strim((char*) value, "\"", R_TRIM_BOTH));
                    } else if (*value == 'W' && value[1] == '/' && value[2] == '"') {
                        //  Weak ETag: strip W/ prefix and quotes
                        web->ifMatch = sclone(strim((char*) value + 2, "\"", R_TRIM_BOTH));
                    } else {
                        //  Date format - parse it into web->since (will be used for conditional range)
                        web->since = parseDateHeader(value);
                    }
                    web->ifRange = 1;

                } else if (scaselessmatch(key, "if-unmodified-since")) {
                    web->unmodifiedSince = parseDateHeader(value);
                    if (web->unmodifiedSince > 0) {
                        web->ifUnmodified = 1;
                    }
                }

            } else if (c == 'h' && scaselessmatch(key, "host")) {
                //  RFC 9112 3.2: more than one Host is a bad request
                if (hasHost) {
                    webError(web, -400, "Duplicate Host header");
                    return 0;
                }
                hasHost = 1;

            } else if (c == 'l' && scaselessmatch(key, "last-event-id")) {

                if (sparseUint(value, 10, MAXINT64, &idEnd, &eventId) == 0 && *idEnd == '\0') {
                    web->lastEventId = (ssize) eventId;
                }

            } else if (c == 'o' && scaselessmatch(key, "origin")) {
                web->origin = value;

            } else if (c == 'r' && scaselessmatch(key, "range")) {
                /*
                    One Range only. parseRangeHeader assigns the list head unconditionally, so a
                    repeat orphaned the whole previous list - unrecoverable for the life of the
                    process - and left the framing of two disagreeing headers ambiguous.
                 */
                if (hasRange) {
                    webError(web, -400, "Duplicate Range header");
                    return 0;
                }
                hasRange = 1;
                value = sclone(value);
                if (!parseRangeHeader(web, value)) {
                    rFree(value);
                    webError(web, 400, "Invalid Range header");
                    return 0;
                }
                rFree(value);

            } else if (c == 't' && scaselessmatch(key, "transfer-encoding")) {
                if (hasTE) {
                    webError(web, -400, "Duplicate Transfer-Encoding");
                    return 0;
                }
                /*
                    RFC 9112 6.1: Transfer-Encoding is a comma-separated list of codings, and
                    "chunked" must be final and appear exactly once. A recipient that cannot
                    decode the codings MUST respond 501 and MUST NOT treat the header as absent.

                    Matching the whole value against "chunked" meant "gzip, chunked" and
                    "chunked, chunked" were silently ignored - the body was then treated as
                    zero-length while a front-end decoded it, which is a smuggling desync. It
                    also left hasTE clear, so such a request slipped past the Content-Length
                    conflict check below and the Content-Length was honoured instead.

                    hasTE is therefore set on the presence of the header, not on the value.
                 */
                hasTE = 1;
                if (scaselessmatch(strim(value, " \t", R_TRIM_BOTH), "chunked")) {
                    web->chunked = WEB_CHUNK_START;
                } else {
                    webError(web, -501, "Unsupported transfer coding");
                    return 0;
                }
            } else if (c == 'u' && scaselessmatch(key, "upgrade")) {
                web->upgrade = value;
            }
        }
    }
    if (web->uploads || web->put) {
        if (web->rxLen > web->host->maxUpload) {
            webError(web, -413, "Request upload body content-length is too big");
            return 0;
        }
    } else {
        if (web->rxLen > web->host->maxBody) {
            webError(web, -413, "Request content-length is too big");
            return 0;
        }
    }
    if (hasCL && hasTE) {
        webNetError(web, "Cannot have both Content-Length and Transfer-Encoding");
        return 0;
    }
    //  RFC 9112 6.1: Transfer-Encoding on an HTTP/1.0 request is faulty framing
    if (hasTE && web->http10) {
        webError(web, -400, "Transfer-Encoding is not supported with HTTP/1.0");
        return 0;
    }
    /*
        Switch protocols only when Connection lists upgrade and the request keeps the connection open (RFC 9110
        7.8).
     */
    if (!upload && web->upgrade && (!web->connUpgrade || web->close)) {
        web->upgrade = 0;
    }
    /*
        Refuse unframed multipart bodies: without Content-Length or Transfer-Encoding the request has no body (RFC
        9112 6.3). Part headers are exempt.
     */
    if (!upload && web->uploads && !web->chunked && web->rxLen < 0) {
        webError(web, -411, "Multipart upload requires Content-Length or Transfer-Encoding");
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

    for (cp = start; cp < end; ) {
        if (scaselessmatch(cp, name)) {
            cp += slen(name) + 1;
            while (isWhite(*cp)) cp++;
            value = cp;
            break;
        }

        cp += slen(cp) + 1;
        cp += slen(cp);
        while (cp < end && *cp == '\0') cp++;
    }
    return value;
}

PUBLIC bool webGetNextHeader(Web *web, cchar **pkey, cchar **pvalue)
{
    cchar *cp, *end, *start;

    assert(pkey);
    assert(pvalue);

    if (!pkey || !pvalue) {
        return 0;
    }
    if (*pvalue) {
        // Trailing whitespace produces more than two terminating NULs.
        end = rGetBufEnd(web->rxHeaders);
        for (start = *pvalue + slen(*pvalue); start < end && *start == '\0'; start++) {
        }
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
    Not used for streamed, websockets, or PUT requests
 */
PUBLIC int webReadBody(Web *web)
{
    WebRoute *route;
    RBuf *buf;
    ssize nbytes;
    Ticks receiveBy;

    route = web->route;
    /*
        bodyRead rather than a test of the framing state. web->chunked is a state-machine position,
        not a boolean, and its terminal value WEB_CHUNK_EOF is still truthy, so "no body remains"
        was false for a fully-read chunked request. A second call then re-parsed the same body and
        orphaned the first parse. Any caller after the body has been read - a directory redirect,
        a route redirect, a handler - is now a no-op whatever the framing.
     */
    /* Buffer and validate PUT bodies described by signatures; otherwise stream them to the handler. */
    if (web->bodyRead || (route && route->stream) || web->webSocket ||
        (web->put && !(web->signature >= 0 && isValidated(web))) ||
        (web->rxRemaining <= 0 && !web->chunked)) {
        // Delay reading request body
        return 0;
    }
    if (!web->body) {
        web->body = rAllocBuf(ME_BUFSIZE);
    }
    /*
        Bound the whole read, not each wait within it. webConsumeBody calls webUpdateDeadline for
        every byte taken from the buffer, which resets the deadline to the inactivity window and,
        byte by byte, out to the full request timeout - so a peer sending one byte at a time held
        this fiber, and therefore one of the fiber pool's connection slots, for minutes while
        sending almost nothing. receiveTimeout is measured from the start of the request and is not
        refreshed, so the body either arrives inside the budget or the request fails.

        Only this path is bounded, and only up to web.limits.body: the guard above has already
        returned for streamed routes, WebSockets and PUT, which read at the peer's pace by design
        and where a large upload over a slow link is legitimate traffic of the same shape.
     */
    receiveBy = rGetTimeouts() ? web->started + web->host->receiveTimeout : 0;

    buf = web->body;
    do {
        if (receiveBy) {
            web->deadline = min(web->deadline, receiveBy);
        }
        rReserveBufSpace(buf, ME_BUFSIZE);
        nbytes = webRead(web, rGetBufEnd(buf), rGetBufSpace(buf));
        if (nbytes < 0) {
            return R_ERR_CANT_READ;
        }
        rAdjustBufEnd(buf, nbytes);
        if (rGetBufLength(buf) > (size_t) web->host->maxBody) {
            webError(web, -413, "Request is too big");
            return R_ERR_CANT_READ;
        }
    } while (nbytes > 0 && web->rxRemaining > 0);
    rAddNullToBuf(buf);
    if (buf->end >= buf->endbuf) {
        //  processBody parses the body as a C string, so never hand it an unterminated block
        webError(web, -413, "Request is too big");
        return R_ERR_CANT_READ;
    }
    web->bodyRead = 1;

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
    //  Never orphan an earlier parse: a re-entry must replace it, not leak it
    if (web->jsonBody) {
        jsonFree(web->vars);
        if ((web->vars = webParseJson(web)) == 0) {
            return webError(web, 400, "JSON body is malformed");
        }
    } else if (web->formBody) {
        jsonFree(web->vars);
        web->vars = jsonAlloc();
        return webParseForm(web);
    }
    return 0;
}

static int processQuery(Web *web)
{
    web->qvars = jsonAlloc();
    return webParseQuery(web);
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
    webWriteResponseString(web, 200, NULL);
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
PUBLIC void webBuffer(Web *web, size_t size)
{
    /* Stop buffering once headers fix framing; buffering can no longer compute Content-Length. */
    if (web->wroteHeaders) {
        return;
    }
    if (size <= 0) {
        size = ME_BUFSIZE;
    }
    size = max(size, (size_t) web->host->maxBuffer);
    if (web->buffer) {
        if (rGetBufSize(web->buffer) < size) {
            rGrowBuf(web->buffer, size);
        }
    } else {
        web->buffer = rAllocBuf(size);
    }
    web->buffering = 1;
}

/*
    Determine if this response may be stored and reused by a cache at all.
    Cacheability is a property of the response, not only of the route: an error, an authentication
    challenge, a credentialed body or a response minting a session cookie must never be stored,
    however the route is configured. A cached 401 locks the legitimate user out for the route's
    maxAge, and a cached authenticated 200 is served to the next anonymous client.
 */
/* Authenticated, session-bound and token-bearing responses must not enter shared caches. */
static bool credentialedResponse(Web *web)
{
    return web->authenticated || web->session || rLookupName(web->txHeaders, "Set-Cookie") ||
           rLookupName(web->txHeaders, WEB_XSRF_HEADER);
}

static bool cacheableResponse(Web *web)
{
    if (!(web->status == 200 || web->status == 203 || web->status == 204 ||
          web->status == 206 || web->status == 304)) {
        return false;
    }
    if (!web->get && !web->head) {
        return false;
    }
    if (credentialedResponse(web)) {
        return false;
    }
    return true;
}

/*
    Determine if cache control headers should be set for this request
 */
static bool shouldCacheControl(Web *web, WebRoute *route)
{
    cchar *ext;

    if (route->cacheMaxAge == 0 && !route->cacheDirectives) {
        //  Check if cache control is configured (maxAge > 0 or directives set)
        return false;
    }
    if (!cacheableResponse(web)) {
        return false;
    }
    if (!route->extensions) {
        //  If no extensions specified, match all requests on this route
        return true;
    }
    /*
        Check file extension - skip the leading dot before looking it up
     */
    ext = web->ext;
    if (!ext || !ext[1] || !rLookupName(route->extensions, ext + 1)) {
        return false;
    }
    return true;
}

/*
    Set client cache control headers
 */
PUBLIC void webSetCacheControlHeaders(Web *web)
{
    WebRoute *route;
    RBuf *buf;
    time_t expires;
    bool hasNoCache, hasNoStore, hasPrivate;

    route = web->route;
    if (!route || !shouldCacheControl(web, route)) {
        /*
            Without route policy, explicitly forbid caching errors, challenges and credential/session-bound
            responses.
         */
        if ((web->status >= 400 || credentialedResponse(web)) && !rLookupName(web->txHeaders, "Cache-Control")) {
            webAddHeaderStaticString(web, "Cache-Control", "no-store");
        }
        return;
    }
    if (rLookupName(web->txHeaders, "Cache-Control")) {
        //  The handler set its own policy. Appending the route's would send two that disagree.
        return;
    }
    hasNoCache = route->cacheDirectives && scontains(route->cacheDirectives, "no-cache");
    hasNoStore = route->cacheDirectives && scontains(route->cacheDirectives, "no-store");
    hasPrivate = route->cacheDirectives && scontains(route->cacheDirectives, "private");
    /*
        Build Cache-Control header value
        Always prefix directives with ", " then skip the first comma
     */
    buf = rAllocBuf(256);
    if (route->cacheDirectives) {
        rPutToBuf(buf, ", %s", route->cacheDirectives);
    }
    if (route->cacheMaxAge > 0) {
        //  Add max-age if specified
        rPutToBuf(buf, ", max-age=%d", route->cacheMaxAge);
    }
    //  Set Cache-Control header
    if (rGetBufLength(buf) > 0) {
        //  Skip the leading ", " by adjusting buffer start
        rAdjustBufStart(buf, 2);
        //  Convert buffer to string and free buffer in one operation
        webAddHeaderDynamicString(web, "Cache-Control", rBufToStringAndFree(buf));
    } else {
        rFreeBuf(buf);
    }

    /* Merge request-dependent headers into Vary for shared caches. Private caches need no shared key. */
    if (!hasPrivate && !hasNoStore) {
        webAddVary(web, "Cookie");
        webAddVary(web, "Authorization");
    }

    /*
        Set Expires and Pragma headers for HTTP/1.0 compatibility
        HTTP/1.1+ clients understand Cache-Control and don't need these headers
     */
    if (web->http10) {
        //  An HTTP/1.0 cache does not understand private, so a private response gets no Expires
        if (route->cacheMaxAge > 0 && !hasNoCache && !hasNoStore && !hasPrivate) {
            expires = time(0) + route->cacheMaxAge;
            webAddHeaderDynamicString(web, "Expires", webHttpDate(expires));

        } else if (hasNoCache) {
            //  Set past expiry for no-cache
            webAddHeaderStaticString(web, "Expires", "0");
            webAddHeaderStaticString(web, "Pragma", "no-cache");
        }
    }
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file src/io.c ************/

/*
    io.c - I/O for the web server

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/


/************************************ Forwards *********************************/

static ssize bodyAvailable(Web *web);
static int decodeChunks(Web *web);
static ssize fillBuffer(Web *web, size_t toRead);
static char *findCrlf(char *buf, size_t len, bool *bad);
static char *findPatternFrom(RBuf *buf, cchar *pattern, size_t patLen, size_t fromOffset, size_t len);
static bool isprintable(cchar *s, size_t len);
static bool isTrailerField(cchar *line, size_t len);
static ssize readSocketBlock(Web *web, size_t desiredSize);
static void sendRedirect(Web *web, int status, cchar *location);
static int writeChunkDivider(Web *web, size_t bufsize);

/*
    Bound on a chunk-size line, including any chunk extension. Legal lines are a few hex digits;
    a line this long is not a size, it is an attempt to pin the buffer.
 */
#define WEB_CHUNK_LINE 128

/************************************* Code ***********************************/
/*
    Read request body data into a buffer and return the number of bytes read.
    This is how users read the request body into their own buffers.
    The web->rxRemaining indicates the number of bytes yet to read.
    This reads through the web->rx low-level buffer.
    This will block the current fiber until some data is read.
 */
PUBLIC ssize webRead(Web *web, char *buf, size_t bufsize)
{
    ssize nbytes;

    if ((nbytes = readSocketBlock(web, bufsize)) < 0) {
        if (web->rxRemaining > 0) {
            return webNetError(web, "Cannot read from socket");
        }
        web->close = 1;
        return 0;
    }
    if (nbytes == 0) {
        return 0;
    }
    memcpy(buf, web->rx->start, (size_t) nbytes);
    webConsumeBody(web, (size_t) nbytes);
    return nbytes;
}

/*
    Universal low-level socket read routine into the request body buffer.
    This is how the server reads the request body into the web->rx buffer.
 */
static ssize readSocket(Web *web, size_t toRead, Ticks deadline)
{
    RBuf  *bp;
    ssize nbytes;

    bp = web->rx;
    if ((nbytes = rReadSocket(web->sock, bp->end, toRead, deadline)) < 0) {
        /*
            Distinguish "you were too slow" from "the connection broke". Both arrive here as a
            negative read, and funnelling them together is why every timeout closed silently and
            was indistinguishable at the client from a crash or a network failure.
         */
        if (deadline && rGetTicks() >= deadline) {
            web->timedOut = 1;
        }
        return R_ERR_CANT_READ;
    }
    rAdjustBufEnd(bp, nbytes);
    web->rxRead += nbytes;
    return nbytes;
}

/*
    Request body bytes ready at the front of the rx buffer. For a chunked body this is the decoded
    region: the raw bytes behind it still carry their framing and are not body data yet. For a body
    of known length it is bounded by the bytes remaining, so a pipelined request buffered behind the
    body is never handed out as body data. Before the headers are parsed nothing is known and the
    whole buffer is available.
 */
PUBLIC size_t webBufferedBody(Web *web)
{
    if (web->chunked) {
        return (size_t) web->rxDecoded;
    }
    return min(rGetBufLength(web->rx), (size_t) web->rxRemaining);
}

/*
    Body bytes ready at the front of the rx buffer, after decoding any raw chunked data that
    arrived with the headers or behind the region already decoded. Negative on a framing error,
    after the error response has been issued.
 */
static ssize bodyAvailable(Web *web)
{
    if (web->chunked && decodeChunks(web) < 0) {
        return R_ERR_BAD_REQUEST;
    }
    return (ssize) webBufferedBody(web);
}

/*
    Consume body bytes from the front of the rx buffer after they have been used.
 */
PUBLIC void webConsumeBody(Web *web, size_t nbytes)
{
    if (nbytes == 0) {
        return;
    }
    rAdjustBufStart(web->rx, (ssize) nbytes);
    if (web->chunked) {
        web->rxDecoded -= (ssize) nbytes;
        if (web->rxDecoded == 0 && web->chunked == WEB_CHUNK_EOF) {
            web->rxRemaining = 0;
        }
    } else {
        web->rxRemaining -= (Offset) nbytes;
    }
    webUpdateDeadline(web);
}

/* Find CRLF within len; return NULL if incomplete. Set bad on bare CR/LF to prevent ambiguous framing. */
static char *findCrlf(char *buf, size_t len, bool *bad)
{
    char *cp, *end;

    *bad = 0;
    end = &buf[len];
    for (cp = buf; cp < end; cp++) {
        if (*cp == '\r') {
            if (&cp[1] >= end) {
                return 0;
            }
            if (cp[1] == '\n') {
                return cp;
            }
            *bad = 1;
            return 0;
        } else if (*cp == '\n') {
            *bad = 1;
            return 0;
        }
    }
    return 0;
}

/*
    Require token-name, colon, and no NUL in trailers (RFC 9112 7.1.2).
 */
static bool isTrailerField(cchar *line, size_t len)
{
    cchar *cp, *end;

    end = &line[len];
    for (cp = line; cp < end && *cp != ':'; cp++) {
        if (!isalnum((uchar) * cp) && !schr("!#$%&'*+-.^_`|~", *cp)) {
            return 0;
        }
    }
    if (cp == line || cp >= end) {
        return 0;
    }
    return memchr(cp, '\0', (size_t) (end - cp)) == 0;
}

/*
    Decode chunks in place after the decoded region, resuming incomplete framing on later reads. Require bounded
    hexadecimal sizes and valid extensions/trailers (RFC 9112 7.1). Leave pipelined bytes untouched. Return 0, or
    negative after an error response.
 */
static int decodeChunks(Web *web)
{
    RBuf   *bp;
    char   *raw, *eol, *ep;
    int64  value, maxChunk;
    size_t rawLen, lineLen, limit;
    ssize  nbytes;
    bool   bad;
    int    rc;

    bp = web->rx;
    maxChunk = (web->uploads || web->put) ? web->host->maxUpload : web->host->maxBody;
    while (web->chunked != WEB_CHUNK_EOF) {
        raw = &bp->start[web->rxDecoded];
        rawLen = (size_t) (bp->end - raw);
        if (rawLen == 0) {
            break;
        }
        if (web->chunked == WEB_CHUNK_DATA) {
            nbytes = (ssize) min(rawLen, (size_t) web->chunkRemaining);
            web->rxDecoded += nbytes;
            web->chunkRemaining -= nbytes;
            if (web->chunkRemaining == 0) {
                web->chunked = WEB_CHUNK_END;
            }
            continue;
        }
        /*
            The remaining states each expect a CRLF-terminated line. A chunk-size line is short by
            nature, so bound it tightly; a trailer section is bounded like the headers.
         */
        limit = (web->chunked == WEB_CHUNK_TRAILER) ? (size_t) web->host->maxHeader : WEB_CHUNK_LINE;
        if ((eol = findCrlf(raw, min(rawLen, limit), &bad)) == 0) {
            if (bad || rawLen >= limit) {
                return webError(web, -400, "Bad chunk specification");
            }
            //  Need more data
            break;
        }
        lineLen = (size_t) (eol - raw) + 2;

        //  Framing and trailers are read and buffered like the body, so they share its limit
        web->rxFraming += (ssize) lineLen;
        if (web->rxFraming > maxChunk) {
            return webError(web, -413, "Chunk framing exceeds the body limit");
        }
        if (web->chunked == WEB_CHUNK_START) {
            *eol = '\0';
            rc = sparseUint(raw, 16, maxChunk, &ep, &value);
            *eol = '\r';
            if (rc < 0 || (ep != eol && *ep != ';')) {
                return webError(web, -400, "Bad chunk specification");
            }
            if (value == 0) {
                web->chunked = WEB_CHUNK_TRAILER;
            } else {
                web->chunkRemaining = value;
                web->chunked = WEB_CHUNK_DATA;
            }

        } else if (web->chunked == WEB_CHUNK_END) {
            if (lineLen != 2) {
                return webError(web, -400, "Bad chunk data");
            }
            web->chunked = WEB_CHUNK_START;

        } else if (lineLen != 2 && !isTrailerField(raw, lineLen - 2)) {
            return webError(web, -400, "Bad chunk trailer");

        } else if (lineLen == 2) {
            /*
                Empty line ends the trailer section and the body. The body is not finished with
                until the decoded data has been consumed as well, so rxRemaining stays set while
                any is pending and webConsumeBody clears it.
             */
            web->chunked = WEB_CHUNK_EOF;
            if (web->rxDecoded == 0) {
                web->rxRemaining = 0;
            }
        }
        //  Close the data up over the framing line
        memmove(raw, &raw[lineLen], rawLen - lineLen);
        rAdjustBufEnd(bp, -(ssize) lineLen);
    }
    return 0;
}

/*
    Answer a request that ran out of time before any response was written.

    RFC 9110 section 15.5.9: a server that times out waiting for a request should send 408 and
    close. Without it the client sees a bare TCP close, indistinguishable from a crash or a dropped
    link, so a retry loop that would back off on a 408 retries immediately against a server already
    under timeout pressure, and an operator debugging dropped requests has no signal at either end.

    This must run before webNetError, which closes the socket: once it is closed there is nothing
    left to answer on, and that is why every timeout path was silent.

    Two cases stay silent, deliberately. Once response headers are on the wire a status cannot be
    prepended. And an idle keep-alive connection with no request in flight never reaches here - it
    times out in the wait handler, which RFC 9112 section 9.5 allows to close without a response.
 */
static void timeoutResponse(Web *web)
{
    if (!web->timedOut || web->wroteHeaders || rIsSocketClosed(web->sock)) {
        return;
    }
    web->close = 1;
    webWriteResponseString(web, -408, "Request timed out");
}

/*
    Read from the socket into the rx buffer. Reads up to toRead bytes, growing the buffer as
    required, and decodes any chunk framing on what arrived. Returns the number of bytes read from
    the socket or negative on error, after issuing the error response.
 */
static ssize fillBuffer(Web *web, size_t toRead)
{
    RBuf  *bp;
    ssize nbytes;

    bp = web->rx;
    rCompactBuf(bp);
    //  Never read past the end of a body of known length
    if (!web->chunked && (size_t) web->rxRemaining < toRead) {
        toRead = (size_t) web->rxRemaining;
    }
    rReserveBufSpace(bp, toRead);
    toRead = min(toRead, rGetBufSpace(bp));
    if ((nbytes = readSocket(web, toRead, web->deadline)) < 0) {
        timeoutResponse(web);
        return webNetError(web, "Cannot read from socket");
    }
    if (web->chunked && decodeChunks(web) < 0) {
        return R_ERR_BAD_REQUEST;
    }
    return nbytes;
}

/*
    Make request body data available at the front of the rx buffer, reading from the socket as
    required. Returns the number of bytes available up to desiredSize, 0 at the end of the body, or
    negative on error. The data is not consumed: callers call webConsumeBody() when done with it.
 */
static ssize readSocketBlock(Web *web, size_t desiredSize)
{
    ssize  available;
    size_t toRead;

    while ((available = bodyAvailable(web)) == 0) {
        if (web->rxRemaining == 0) {
            return 0;
        }
        //  Read as much as possible to minimize the number of socket reads
        toRead = max(desiredSize, WEB_BUF_BOOST_4X);
        if (fillBuffer(web, toRead) < 0) {
            return R_ERR_CANT_READ;
        }
    }
    if (available < 0) {
        return available;
    }
    return min(available, (ssize) desiredSize);
}

/*
    Read request body data directly from the rx buffer (zero-copy).
    Fills buffer and sets *dataPtr to the data location. Consumes internally before returning.
    Returns bytes available, or 0 on EOF, or negative on error.

    Usage pattern:
        char *ptr;
        ssize nbytes;
        while ((nbytes = webReadDirect(web, &ptr, WEB_BUF_BOOST_4X)) > 0) {
            write(fd, ptr, nbytes);
        }
 */
PUBLIC ssize webReadDirect(Web *web, char **dataPtr, size_t desiredSize)
{
    ssize nbytes;

    if ((nbytes = readSocketBlock(web, desiredSize)) <= 0) {
        *dataPtr = NULL;
        return nbytes;
    }
    //  Save pointer before consuming
    *dataPtr = web->rx->start;
    webConsumeBody(web, (size_t) nbytes);
    return nbytes;
}

/*
    Read response data until a designated pattern is read up to a limit.
    Wrapper over webBufferUntil() used to read chunk delimiters.
    If a user buffer is provided, data is read into it, up to the limit and the rx buffer is adjusted.
    If the user buffer is not provided, don't copy into the user buffer and don't adjust the rx buffer.
    Always return the number of read up to and including the pattern.
    If the pattern is not found inside the limit, returns negative error.
    Note: this routine may over-read and data will be buffered for the next read.
 */
PUBLIC ssize webReadUntil(Web *web, cchar *until, char *buf, size_t limit)
{
    RBuf  *bp;
    ssize len, nbytes;

    assert(buf);
    assert(limit);
    assert(until);

    bp = web->rx;

    if ((nbytes = webBufferUntil(web, until, limit)) < 0) {
        return R_ERR_CANT_READ;
    }
    if (nbytes == 0) {
        // Pattern not found before limit
        return R_ERR_CANT_FIND;
    }
    if (buf && nbytes > 0) {
        /*
            Refuse rather than truncate. Copying min(nbytes, limit) while returning nbytes left the
            tail of an over-long line in the buffer, where the caller then consumed it as data -
            the buffer and the returned length disagreed, which is a desync waiting for a caller.
         */
        if ((size_t) nbytes > limit) {
            return R_ERR_WONT_FIT;
        }
        len = (ssize) nbytes;
        memcpy(buf, bp->start, (size_t) len);
        webConsumeBody(web, (size_t) len);
    }
    return nbytes;
}

/*
    Read until the specified pattern is seen or until the size limit.
    Reading the headers may also read all the body data and (WARNING) any pipelined request
    following. Only the request body region of the buffer is scanned: a chunked body is decoded
    as it arrives and a body of known length is never scanned past its end, so the pattern is
    matched against body bytes alone.
    Return the total number of buffered bytes up to and including the requested pattern.
    Return zero if the pattern is not found before the limit, negative if the body ends before
    the pattern is seen or on a read error.

    Uses incremental scanning: tracks how much has been scanned and only scans new data
    plus a safety overlap of pattern length to handle patterns split across reads.
 */
PUBLIC ssize webBufferUntil(Web *web, cchar *pattern, size_t limit)
{
    RBuf   *bp;
    char   *end;
    ssize  available;
    size_t patLen, scanned, scanFrom;

    assert(web);
    assert(pattern);

    bp = web->rx;
    patLen = slen(pattern);
    scanned = 0;

    while (1) {
        if ((available = bodyAvailable(web)) < 0) {
            return available;
        }
        //  Scan from (scanned - patLen) to handle patterns split across reads
        scanFrom = (scanned > patLen) ? scanned - patLen : 0;

        if ((end = findPatternFrom(bp, pattern, patLen, scanFrom, (size_t) available)) != 0) {
            break;
        }
        if ((size_t) available >= limit) {
            //  Pattern not found before limit
            return 0;
        }
        if (web->chunked == WEB_CHUNK_EOF || web->rxRemaining <= available) {
            //  The body has ended without the pattern
            return R_ERR_CANT_FIND;
        }
        //  Mark current buffer as fully scanned before reading more
        scanned = (size_t) available;
        if (fillBuffer(web, limit - (size_t) available) < 0) {
            return R_ERR_CANT_READ;
        }
    }
    //  Return data including "until" pattern
    return &end[patLen] - bp->start;
}

/*
    Find pattern in buffer starting from a given offset. Only used by webBufferUntil.
    The fromOffset parameter enables incremental scanning - only scanning new data
    plus a safety overlap to handle patterns split across socket reads.
    Return a pointer to the pattern in the buffer or NULL if not found.
 */
static char *findPatternFrom(RBuf *buf, cchar *pattern, size_t patLen, size_t fromOffset, size_t len)
{
    char *cp, *endp, *searchStart;

    assert(buf);

    if (len < patLen) {
        return 0;
    }
    searchStart = buf->start + fromOffset;
    endp = buf->start + (len - patLen) + 1;

    if (searchStart >= endp) {
        return 0;
    }
    for (cp = searchStart; cp < endp; cp++) {
        if ((cp = (char*) memchr(cp, *pattern, (size_t) (endp - cp))) == 0) {
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
    WebHost *host;
    Ticks   remaining;
    RName   *header;
    RBuf    *buf;
    cchar   *connection, *protocol;
    char    *redacted;
    ssize   nbytes;
    int     status;

    host = web->host;
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
    /*
        The count is origin zero and is incremented by one after each request.
     */
    if (web->count >= host->maxRequests) {
        web->close = 1;
    }
    webAddHeaderDynamicString(web, "Date", webHttpDate(time(0)));

    if (web->upgrade && status == 101) {
        connection = "Upgrade";
    } else if (web->close) {
        connection = "close";
    } else {
        connection = "keep-alive";
        remaining = host->requestTimeout - (rGetTicks() - web->connectionStarted);
        webAddHeader(web, "Keep-Alive", "timeout=%lld, max=%lld",
                     (long long) (remaining / TPS), (long long) (host->maxRequests - web->count));
    }
    webAddHeaderStaticString(web, "Connection", connection);

    web->chunkedTx = 0;
    if (!((100 <= status && status <= 199) || status == 204 || status == 304)) {
        //  Server must not emit a content length header for 1XX, 204 and 304 status
        if (web->txLen < 0) {
            /*
                HTTP/1.0 has no chunked transfer coding, so a 1.0 client parses the chunk-size lines
                as body content. Frame by connection close instead - web->close is already set for
                1.0 in parseRequestLine.
             */
            if (!web->http10) {
                web->chunkedTx = 1;
                webAddHeaderStaticString(web, "Transfer-Encoding", "chunked");
            }
        } else {
            web->txRemaining = web->txLen;
            webAddHeader(web, "Content-Length", "%lld", (long long) web->txLen);
        }
    }
    if (web->redirect) {
        webAddHeaderStaticString(web, "Location", web->redirect);
    }
    if (!rLookupName(web->txHeaders, "Content-Type")) {
        if (!web->mime && web->ext) {
            web->mime = rLookupName(host->mimeTypes, web->ext);
        }
        if (!web->mime && web->txLen != 0) {
            web->mime = "application/octet-stream";
        }
        if (web->mime) {
            webAddHeaderStaticString(web, "Content-Type", web->mime);
        }
    }

    //  Set client-side cache control headers based on route configuration
    webSetCacheControlHeaders(web);

#if ME_DEBUG && KEEP
    /*
        If testing CORS, allow origin header to use the request origin
     */
    if (smatch(rLookupName(web->txHeaders, "Access-Control-Allow-Origin"), "dynamic") == 0) {
        webAddAccessControlHeader(web);
    }
#endif
    /*
        Emit HTTP response line
     */
    //  Always a server owned constant. Never echo the token the client sent.
    protocol = web->http10 ? "HTTP/1.0" : "HTTP/1.1";

    buf = rAllocBuf(1024);
    rPutStringToBuf(buf, protocol);
    rPutStringToBuf(buf, " ");
    rPutIntToBuf(buf, status);
    rPutStringToBuf(buf, " ");
    rPutStringToBuf(buf, webGetStatusMsg(status));
    rPutStringToBuf(buf, "\r\n");
    if (!rEmitLog("trace", "web")) {
        rTrace("web", "%s", rGetBufStart(buf));
    }

    /*
        Emit response headers
     */
    for (ITERATE_NAMES(web->txHeaders, header)) {
        rPutStringToBuf(buf, header->name);
        rPutStringToBuf(buf, ": ");
        rPutStringToBuf(buf, (cchar*) header->value);
        rPutStringToBuf(buf, "\r\n");
    }
    if (host->flags & WEB_SHOW_RESP_HEADERS) {
        redacted = webRedactHeaders(rBufToString(buf));
        rLog("raw", "web", "Response >>>>\n\n%s\n", redacted);
        rFree(redacted);
    }
    if (!web->chunkedTx || web->head) {
        /*
            Delay adding if using transfer encoding. This optimization eliminates a write per chunk.
            A HEAD response writes no chunk framing at all, so its terminator has to be emitted here
            or the header block runs straight into the next response on the connection.
         */
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
    /*
        Baseline response headers, emitted unless the configuration names them.

        These are in code rather than in the shipped web.json5 on purpose: an integrator who writes
        their own configuration from scratch - which is the normal case, since the templates are a
        starting point - would otherwise silently lose them. Configuration becomes a way to
        override a secure default rather than the only thing that supplies one, which is the right
        direction for a default to travel.
     */
    static cchar *baseline[][2] = {
        { "X-Content-Type-Options", "nosniff" },
        { "X-Frame-Options",        "SAMEORIGIN" },
        { "Referrer-Policy",        "no-referrer" },
        { 0, 0 }
    };
    WebHost      *host;
    Json         *json;
    JsonNode     *header;
    int          i;

    host = web->host;
    if (host->headers >= 0) {
        json = host->config;
        for (ITERATE_JSON_KEY(json, host->headers, NULL, header, id)) {
            webAddHeaderStaticString(web, header->name, header->value);
        }
    }
    for (i = 0; baseline[i][0]; i++) {
        if (!rLookupName(web->txHeaders, baseline[i][0])) {
            webAddHeaderStaticString(web, baseline[i][0], baseline[i][1]);
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
    rAddDuplicateName(web->txHeaders, key, value, R_DYNAMIC_VALUE);
}

PUBLIC void webAddHeaderStaticString(Web *web, cchar *key, cchar *value)
{
    rAddDuplicateName(web->txHeaders, key, (void*) value, R_STATIC_VALUE);
}

/* Allow absent Origin, same-origin requests, or explicitly configured origins. A configured * allows all origins. */
PUBLIC bool webMatchOrigin(Web *web, int originsId)
{
    JsonNode *child;
    cchar    *origin, *hostHeader;
    char     *hostname, *expect;
    bool     ok;

    origin = web->origin;
    if (origin == 0 || *origin == '\0') {
        return 1;
    }
    if (originsId >= 0) {
        for (ITERATE_JSON_ID(web->host->config, originsId, child, id)) {
            if (smatch(child->value, "*") || smatch(child->value, origin)) {
                return 1;
            }
        }
        return 0;
    }
    hostname = 0;
    if ((hostHeader = webGetHeader(web, "host")) == 0 || *hostHeader == '\0') {
        if ((hostname = webGetHostName(web)) == 0) {
            return 0;
        }
        hostHeader = hostname;
    }
    expect = sfmt("%s://%s", rIsSocketSecure(web->sock) ? "https" : "http", hostHeader);
    ok = smatch(origin, expect);
    rFree(expect);
    rFree(hostname);
    return ok;
}

/*
    Merge into Vary so caches retain every request-dependent field.
 */
PUBLIC void webAddVary(Web *web, cchar *field)
{
    char  *list, *tok, *next;
    cchar *vary;
    bool  found;

    if (!field || !*field) {
        return;
    }
    if ((vary = rLookupName(web->txHeaders, "Vary")) == 0) {
        webAddHeaderDynamicString(web, "Vary", sclone(field));
        return;
    }
    found = 0;
    list = sclone(vary);
    for (tok = stok(list, ", \t", &next); tok && !found; tok = stok(NULL, ", \t", &next)) {
        found = scaselessmatch(tok, field) || smatch(tok, "*");
    }
    rFree(list);
    if (!found) {
        //  Replace rather than add a second Vary field. The new value is built before the old is freed.
        rAddName(web->txHeaders, "Vary", sfmt("%s, %s", vary, field), R_DYNAMIC_VALUE);
    }
}

/* Echo Origin only when same-origin or allowed by web.cors.origins; otherwise name this host to deny CORS. */
PUBLIC void webAddAccessControlHeader(Web *web)
{
    char  *hostname;
    cchar *schema;

    //  The header depends on Origin, so a cache must key on it
    webAddVary(web, "Origin");
    if (web->origin && *web->origin && webMatchOrigin(web, web->host->corsOrigins)) {
        webAddHeaderStaticString(web, "Access-Control-Allow-Origin", web->origin);
    } else {
        hostname = webGetHostName(web);
        schema = web->sock->tls ? "https" : "http";
        webAddHeader(web, "Access-Control-Allow-Origin", "%s://%s", schema, hostname);
        rFree(hostname);
    }
}

/*
    Write body data. Caller should set bufsize to zero to signify end of body
    if the content length is not defined. webFinalize invokes webWrite with bufsize zero.
    Will write headers if not alread written. Will close the socket on socket errors.
    NOTE: this call will block the fiber in webWriteSocket until the data is written.
    Other fibers continue to run while waiting for the data to drain.
 */
PUBLIC ssize webWrite(Web *web, cvoid *buf, size_t bufsize)
{
    ssize written, buffered;

    if (web->finalized) {
        return 0;
    }
    if (buf == NULL) {
        bufsize = 0;
    } else if (bufsize == 0 || bufsize >= MAXINT) {
        bufsize = slen(buf);
    }
    /*
        Check the response flag; the buffer persists across keep-alive requests.
     */
    buffered = 0;
    if (web->buffering && !web->writingHeaders) {
        if (buf) {
            rPutBlockToBuf(web->buffer, buf, bufsize);
            return (ssize) bufsize;
        }
        /*
            Flush buffered data before finalization so chunked responses still emit their terminating chunk.
         */
        web->buffering = 0;
        if (!web->wroteHeaders) {
            webSetContentLength(web, rGetBufLength(web->buffer));
        }
        if (rGetBufLength(web->buffer) > 0) {
            if ((buffered = webWrite(web, rGetBufStart(web->buffer), rGetBufLength(web->buffer))) < 0) {
                return R_ERR_CANT_WRITE;
            }
        }
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
            if (isprintable(buf, (size_t) written)) {
                if (web->moreBody) {
                    if (write(rGetLogFile(), (char*) buf, (uint) written) < 0) {
                        //  The log is where a failure would be reported, so a failed log write is dropped
                    }
                } else {
                    rLog("raw", "web", "Response Body >>>>\n\n%.*s", (int) written, (char*) buf);
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
    return written + buffered;
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
        rc = webWrite(web, str, 0);
        rFree(str);
        return rc;
    }
    return 0;
}

/*
    Write a transfer-chunk encoded divider if required
 */
static int writeChunkDivider(Web *web, size_t size)
{
    char chunk[24];

    if (!web->chunkedTx || !web->wroteHeaders) {
        return 0;
    }
    /*
        RFC 9112 section 6.3(1): a response to HEAD is terminated by the first empty line after the
        header fields, whatever framing the headers declare. The body suppression in webWrite is
        keyed on a non-empty buffer, so the finalizing write - which carries no data - would
        otherwise emit the terminating chunk here and desynchronize the connection.
     */
    if (web->head) {
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
    web->status = (uint) status;
}

/*
    Emit a single response using a static string and finalize the response output.
    If the status is an error (not 200 or 401), the response will be logged to the error log.
    If status is zero, set the status to 400 and close the socket after issuing the response.
 */
PUBLIC ssize webWriteResponseString(Web *web, int status, cchar *msg)
{
    ssize rc;

    if (web->wroteHeaders) {
        return 0;
    }
    if (!msg) {
        msg = "";
    }
    if (status <= 0) {
        if (status == 0) {
            status = 400;
        } else {
            status = -status;
        }
        web->close = 1;
    }
    web->status = (uint) status;

    if (rIsSocketClosed(web->sock)) {
        rTrace("web", "Socket closed before writing response");
        return R_ERR_CANT_WRITE;
    }
    if (web->error) {
        msg = web->error;
    }
    if (web->buffering) {
        //  This response replaces any partially buffered body
        rFlushBuf(web->buffer);
        web->buffering = 0;
    }
    web->txLen = (ssize) slen(msg);

    webAddHeaderStaticString(web, "Content-Type", "text/plain");

    if (webWriteHeaders(web) < 0) {
        rc = R_ERR_CANT_WRITE;
    } else {
        if (web->status != 204 && !web->head && web->txLen > 0) {
            (void) webWrite(web, msg, (size_t) web->txLen);
        }
        rc = webFinalize(web);
    }
    if (status != 200 && status != 201 && status != 204 && status != 301 && status != 302 && status != 401) {
        rTrace("web", "%s", msg);
    }
    return rc;
}

/*
    Emit a single response with printf-style formatting and finalize the response output.
 */
PUBLIC ssize webWriteResponse(Web *web, int status, cchar *fmt, ...)
{
    va_list ap;
    ssize   rc;
    char    *msg;

    if (web->wroteHeaders) {
        return 0;
    }
    if (!fmt) {
        fmt = "";
    }
    va_start(ap, fmt);
    msg = sfmtv(fmt, ap);
    va_end(ap);

    rc = webWriteResponseString(web, status, msg);
    rFree(msg);
    return rc;
}

PUBLIC ssize webWriteEvent(Web *web, int64 id, cchar *name, cchar *fmt, ...)
{
    va_list ap;
    RBuf    *out;
    char    *buf, *line, *end;
    ssize   nbytes;

    //  A line break in the name would start a new SSE field
    if (name && strpbrk(name, "\r\n")) {
        return R_ERR_BAD_ARGS;
    }
    if (id <= 0) {
        //  The id may be seeded by the client's Last-Event-ID: stop at the largest rather than wrap
        id = web->lastEventId < MAXSSIZE ? ++web->lastEventId : web->lastEventId;
    }
    va_start(ap, fmt);
    buf = sfmtv(fmt, ap);
    va_end(ap);

    if (!web->wroteHeaders) {
        webAddHeaderStaticString(web, "Content-Type", "text/event-stream");
        if (webWriteHeaders(web) < 0) {
            rFree(buf);
            return R_ERR_CANT_WRITE;
        }
    }
    out = rAllocBuf(slen(buf) + 64);
    rPutToBuf(out, "id: %lld\nevent: %s\n", (long long) id, name);

    //  Each line of the data, broken at CR, LF or CRLF, is sent as its own data field
    for (line = buf; ; ) {
        end = line + strcspn(line, "\r\n");
        rPutStringToBuf(out, "data: ");
        rPutBlockToBuf(out, line, (size_t) (end - line));
        rPutCharToBuf(out, '\n');
        if (*end == '\0') {
            break;
        }
        line = end + ((end[0] == '\r' && end[1] == '\n') ? 2 : 1);
    }
    rPutCharToBuf(out, '\n');
    nbytes = webWrite(web, rGetBufStart(out), rGetBufLength(out));
    rFreeBuf(out);
    rFree(buf);
    return nbytes;
}

/*
    Set the response content length.
 */
PUBLIC void webSetContentLength(Web *web, size_t len)
{
    if (len >= 0) {
        web->txLen = (ssize) len;
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
    char *cp, *hostname, ipbuf[40];
    int  port;

    if (web->host->name) {
        //  Canonical host name
        hostname = sclone(web->host->name);
    } else {
        /*
            Hostname from the listening endpoint, e.g. "http://example.com:4240". An endpoint
            that names no host, "http://:80", must not yield ":80": fall through to the
            socket address instead.
         */
        if ((cp = strstr(web->listen->endpoint, "://")) != 0 && cp[3] && cp[3] != ':') {
            hostname = sclone(&cp[3]);
        } else {
            if (rGetSocketAddr(web->sock, (char*) &ipbuf, sizeof(ipbuf), &port) < 0) {
                webError(web, -400, "Missing hostname");
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
        webWriteResponseString(web, 404, "Cannot parse redirection target");
        return;
    }
    if (!port && !scheme && !host && !web->host->name) {
        /*
            Use the configured port; a proxy's backend port may be inaccessible to clients.
         */
        rGetSocketAddr(web->sock, ip, sizeof(ip), &port);
    }
    if (!host) {
        freeHost = webGetHostName(web);
        host = stok(freeHost, ":", &currentPort);
        if (!port && currentPort && (!scheme || smatch(web->scheme, scheme))) {
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
    sendRedirect(web, status, uri);
    rFree(uri);
    rFree(freeHost);
    rFree(buf);
}

/* Use relative Location for local redirects so the client retains its external scheme, host and port. */
PUBLIC void webRedirectLocal(Web *web, int status, cchar *path)
{
    (void) webReadBody(web);

    //  "//host" and "/\host" are network-path references a browser resolves to another site
    if (!path || path[0] != '/' || path[1] == '/' || path[1] == '\\') {
        webError(web, 500, "Bad local redirection target");
        return;
    }
    sendRedirect(web, status, path);
}

static void sendRedirect(Web *web, int status, cchar *location)
{
    rFree(web->redirect);
    web->redirect = webEncode(location);
    web->upgrade = NULL;
    webWriteResponseString(web, status, NULL);
}

/*
    Issue a request error response.
    If status is less than zero, webWriteResponse will issue and ensure the socket is closed after the response is sent,
       and this call will return a negative error code.
    Otherwise, the socket and connection remain usable for further requests and we return zero.
 */
PUBLIC int webError(Web *web, int status, cchar *fmt, ...)
{
    va_list args;

    if (!web->error) {
        va_start(args, fmt);
        web->error = sfmtv(fmt, args);
        va_end(args);
    }
    if (web->wroteHeaders && !web->finalized) {
        /* Close incomplete responses after headers are committed. Already finalized responses remain a no-op. */
        rTrace("web", "%s", web->error);
        web->close = 1;
        webHook(web, WEB_HOOK_ERROR);
        return R_ERR_CANT_COMPLETE;
    }
    webWriteResponseString(web, status, NULL);
    webHook(web, WEB_HOOK_ERROR);
    return status <= 0 ? R_ERR_CANT_COMPLETE : 0;
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
        web->error = sfmtv(fmt, args);
        rTrace("web", "%s", web->error);
    }
    web->status = 550;
    va_end(args);
    rCloseSocket(web->sock);
    webHook(web, WEB_HOOK_ERROR);
    return R_ERR_CANT_COMPLETE;
}

static bool isprintable(cchar *s, size_t len)
{
    cchar *cp;
    int   c;

    if (!s) {
        return 0;
    }
    if (len == 0) {
        return 1;
    }
    for (cp = s; cp < &s[len] && *cp; cp++) {
        c = *cp;
        if ((c > 126) || (c < 32 && c != 10 && c != 13 && c != 9)) {
            return 0;
        }
    }
    return 1;
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file src/session.c ************/

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
static bool evictAnonymousSession(WebHost *host);
static int pruneExpired(WebHost *host);
static void pruneSessions(WebHost *host);

/************************************ Locals **********************************/

PUBLIC int webInitSessions(WebHost *host)
{
    rStopEvent(host->sessionEvent);
    host->sessionEvent = rStartEvent((REventProc) pruneSessions, host, WEB_SESSION_PRUNE);
    return 0;
}

static WebSession *webAllocSession(Web *web, Ticks lifespan)
{
    WebSession *sp;

    assert(web);

    if ((sp = rAllocType(WebSession)) == 0) {
        return 0;
    }
    sp->lifespan = lifespan;
    sp->expires = rGetTicks() + lifespan;
    if ((sp->id = cryptID(32)) == 0) {
        webFreeSession(sp);
        return 0;
    }

    //  Variable names come from callers and may be request data, so the cache owns copies
    if ((sp->cache = rAllocHash(0, R_TEMPORAL_NAME | R_DYNAMIC_VALUE)) == 0) {
        rFree(sp->id);
        rFree(sp);
        return 0;
    }
    if (rAddName(web->host->sessions, sp->id, sp, 0) == 0) {
        webFreeSession(sp);
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

/*
    Drop the server-side session without touching the client's cookie.
 */
static void removeSession(Web *web)
{
    WebSession *session;

    if ((session = webGetSession(web, 0)) != 0) {
        rRemoveName(web->host->sessions, session->id);
        webFreeSession(session);
        web->session = 0;
        rFree(web->sessionId);
        web->sessionId = 0;
    }
}

PUBLIC void webDestroySession(Web *web)
{
    if (webGetSession(web, 0) != 0) {
        webSetCookie(web, web->host->sessionCookie, NULL, "/", 0, 0);
        removeSession(web);
    }
}

/*
    Destroy every session logged in as the user. Requests in flight that hold one are detached first.
 */
PUBLIC void webDestroyUserSessions(WebHost *host, cchar *username)
{
    WebSession *sp;
    Web        *web;
    RName      *np;
    RList      *sessions;
    int        next, wnext;

    if (!host || !username) {
        return;
    }
    sessions = rAllocList(0, 0);
    for (ITERATE_NAMES(host->sessions, np)) {
        sp = (WebSession*) np->value;
        if (smatch(rLookupName(sp->cache, WEB_SESSION_USERNAME), username)) {
            rAddItem(sessions, sp);
        }
    }
    for (ITERATE_ITEMS(sessions, sp, next)) {
        for (ITERATE_ITEMS(host->webs, web, wnext)) {
            if (web->session == sp) {
                web->session = 0;
            }
        }
        rRemoveName(host->sessions, sp->id);
        webFreeSession(sp);
    }
    rFreeList(sessions);
}

/*
    Replace the session. The old session is dropped without emitting a clearing cookie: createSession
    writes a Set-Cookie for the same name immediately afterwards, and same-named response headers do
    not reach the wire in insertion order, so a clear emitted here could arrive last and log the
    client out while leaving an authenticated session behind that nothing can reach or evict.
 */
PUBLIC WebSession *webCreateSession(Web *web)
{
    removeSession(web);
    return createSession(web);
}

/*
    Get the user session by parsing the cookie. If "create" is true, create the session if required.

    The session id is cached on the request, never the WebSession pointer. Sessions are owned by
    host->sessions and are freed by logout, by expiry pruning and by anonymous eviction, none of
    which can see the borrows held by in-flight requests. Ioto is cooperatively scheduled and a
    request yields in webProcessUpload and webReadBody after the session has first been resolved, so
    a cached pointer could be freed under it and every later call would read and write freed memory.
    Re-resolving the id on each call makes the cache a lookup accelerator rather than a lifetime
    assertion: a session freed while a request holds it is simply gone from that request's view.
 */
WebSession *webGetSession(Web *web, int create)
{
    WebSession *session;

    assert(web);

    if (!web->sessionId) {
        web->sessionId = webParseCookie(web, web->host->sessionCookie);
    }
    session = web->sessionId ? rLookupName(web->host->sessions, web->sessionId) : 0;

    if (!session && create) {
        session = createSession(web);
    }
    web->session = session;
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
        /*
            Reclaim before refusing. Pruning is otherwise only periodic, so a burst can hit the
            limit while the table is largely expired.
         */
        pruneExpired(host);
        count = rGetHashLength(host->sessions);
    }
    if (count >= host->maxSessions && !evictAnonymousSession(host)) {
        //  Still full and every session has authenticated - the limit is genuinely reached
        rDebug("session", "Session table full (%d) with authenticated sessions", count);
        webError(web, 429, "Failed to create session");
        return 0;
    }
    if ((session = webAllocSession(web, host->sessionTimeout)) == 0) {
        webError(web, 500, "Failed to create session");
        return 0;
    }
    webSetCookie(web, web->host->sessionCookie, session->id, "/", 0, 0);
    rFree(web->sessionId);
    web->sessionId = sclone(session->id);
    web->session = session;
    return session;
}

#endif /* ME_WEB_SESSIONS */

PUBLIC char *webParseCookie(Web *web, cchar *name)
{
    char *buf, *cookie, *end, *key, *tok, *value, *vtok;

    assert(web);

    //  Limit cookie size for security
    if (web->cookie == 0 || name == 0 || *name == '\0' || slen(web->cookie) > WEB_MAX_COOKIE_SIZE) {
        return 0;
    }
    buf = sclone(web->cookie);
    end = &buf[slen(buf)];
    value = 0;

    for (tok = buf; tok && tok < end; ) {
        cookie = stok(tok, ";", &tok);
        if (cookie == 0) {
            /*
                A header of nothing but delimiters. stok returns NULL here, and passing that to the
                next stok makes it resume from vtok, which nothing has assigned - a read through an
                uninitialised pointer and, when a delimiter is found in the residue, a NUL write
                through it.
             */
            break;
        }
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

#if ME_WEB_SESSIONS
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

    if ((np = rAddName(sp->cache, key, (void*) value, 0)) == 0) {
        rFree(value);
        return 0;
    }
    return np->value;
}

/*
    Remove expired sessions. Timeout is set in web.json.
 */
static int pruneExpired(WebHost *host)
{
    WebSession *sp;
    Ticks      when;
    RName      *np;
    RList      *expired;
    int        removed, next;

    when = rGetTicks();

    //  Collect expired sessions first to avoid modifying hash during iteration
    expired = rAllocList(0, 0);
    for (ITERATE_NAMES(host->sessions, np)) {
        sp = (WebSession*) np->value;
        if (sp->expires <= when) {
            rAddItem(expired, sp);
        }
    }
    //  Now remove and free the expired sessions
    for (ITERATE_ITEMS(expired, sp, next)) {
        rRemoveName(host->sessions, sp->id);
        webFreeSession(sp);
    }
    removed = rGetListLength(expired);
    rFreeList(expired);
    return removed;
}

/*
    Evict the least recently used session that has not authenticated.

    Sessions are created before authentication - webAddSecurityToken creates one to hand an
    anonymous client an XSRF token - so unauthenticated sessions can fill the table and lock out
    every login. Anonymous traffic must not be able to deny the login path, so when the table is
    full an anonymous session yields to a new one. A session that has authenticated is never
    evicted this way; if the table is full of them, the limit is genuinely reached.

    expires slides forward on each access, so the smallest expires is the least recently used.
 */
static bool evictAnonymousSession(WebHost *host)
{
    WebSession *sp, *oldest;
    RName      *np;

    oldest = 0;
    for (ITERATE_NAMES(host->sessions, np)) {
        sp = (WebSession*) np->value;
        if (rLookupName(sp->cache, WEB_SESSION_USERNAME) != 0) {
            //  Authenticated - not a candidate
            continue;
        }
        if (oldest == 0 || sp->expires < oldest->expires) {
            oldest = sp;
        }
    }
    if (oldest == 0) {
        return 0;
    }
    rRemoveName(host->sessions, oldest->id);
    webFreeSession(oldest);
    return 1;
}

static void pruneSessions(WebHost *host)
{
    int count, removed;

    removed = pruneExpired(host);
    count = rGetHashLength(host->sessions);
    if (removed || count) {
        rDebug("session", "Prune %d sessions. Remaining: %d", removed, count);
    }
    host->sessionEvent = rStartEvent((REventProc) pruneSessions, host, WEB_SESSION_PRUNE);
}

/*
    Get a security token to use to mitiate CSRF threats and store it in the session state.
    This will create a security token and save it in session state.
    This will not send the token to the client. Use webAddSecurityToken to send the XSRF token to the client.
    Security tokens are expected to be sent with POST requests to verify the request is not being forged.
    Note: a session is identified by its cookie value ALONE. There is no client-address pairing
    and no other binding - possession of the cookie is sufficient to use the session. This is the
    normal property of a cookie session; address pairing is deliberately not implemented because
    it breaks mobile clients, carrier NAT and load-balanced deployments while being defeated by
    any attacker on the same NAT, which on a device LAN is the likely attacker.

    The controls that do protect the token are: 128 bits from the system RNG, HttpOnly, SameSite,
    Secure on TLS connections, and regeneration on login.
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
        if ((web->securityToken = cryptID(32)) == 0) {
            webError(web, 500, "Cannot generate security token");
            return 0;
        }
        if (!webSetSessionVar(web, WEB_SESSION_XSRF, "%s", web->securityToken)) {
            rFree(web->securityToken);
            web->securityToken = 0;
            if (!web->error) {
                webError(web, 500, "Cannot store security token");
            }
            return 0;
        }
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

    if ((securityToken = webGetSecurityToken(web, recreate)) == 0) {
        return R_ERR_CANT_COMPLETE;
    }
    webAddHeaderDynamicString(web, WEB_XSRF_HEADER, sclone(securityToken));
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
                rDebug("session", "Missing security token in request");
                webAddSecurityToken(web, 1);
                return 0;
            }
        }
        if (!requestToken || !cryptMatch(sessionToken, requestToken)) {
            /*
                Potential CSRF attack. Deny request.
             */
            rDebug("session", "Security token in request does not match session token");
            webAddSecurityToken(web, 1);
            return 0;
        }
    }
    return 1;
}

#else /* !ME_WEB_SESSIONS */
/*
    Keep session APIs linkable when disabled. Login cannot persist; XSRF checks fail closed.
 */
PUBLIC int webInitSessions(WebHost *host)
{
    (void) host;
    return 0;
}

PUBLIC void webFreeSession(WebSession *sp)
{
    (void) sp;
}

PUBLIC void webDestroySession(Web *web)
{
    (void) web;
}

PUBLIC void webDestroyUserSessions(WebHost *host, cchar *username)
{
    (void) host;
    (void) username;
}

PUBLIC WebSession *webCreateSession(Web *web)
{
    (void) web;
    return 0;
}

PUBLIC WebSession *webGetSession(Web *web, int create)
{
    (void) web;
    (void) create;
    return 0;
}

PUBLIC cchar *webGetSessionVar(Web *web, cchar *key, cchar *defaultValue)
{
    (void) web;
    (void) key;
    return defaultValue;
}

PUBLIC void webRemoveSessionVar(Web *web, cchar *key)
{
    (void) web;
    (void) key;
}

PUBLIC cchar *webSetSessionVar(Web *web, cchar *key, cchar *fmt, ...)
{
    (void) web;
    (void) key;
    (void) fmt;
    return 0;
}

PUBLIC cchar *webGetSecurityToken(Web *web, bool recreate)
{
    (void) web;
    (void) recreate;
    return 0;
}

PUBLIC int webAddSecurityToken(Web *web, bool recreate)
{
    (void) web;
    (void) recreate;
    return R_ERR_BAD_STATE;
}

PUBLIC bool webCheckSecurityToken(Web *web)
{
    (void) web;
    return 0;
}
#endif /* ME_WEB_SESSIONS */

/*
    Get a request cookie. The web->cookie contains a list of header cookies.
    A site may submit multiple cookies separated by ";"
 */
PUBLIC char *webGetCookie(Web *web, cchar *name)
{
    char *buf, *cookie, *end, *key, *tok, *value, *vtok;

    //  Limit cookie size for security (consistent with webParseCookie)
    if (web->cookie == 0 || name == 0 || *name == '\0' || slen(web->cookie) > WEB_MAX_COOKIE_SIZE) {
        return 0;
    }
    buf = sclone(web->cookie);
    end = &buf[slen(buf)];
    value = 0;

    for (tok = buf; tok && tok < end; ) {
        cookie = stok(tok, ";", &tok);
        if (cookie == 0) {
            //  Nothing but delimiters - see webParseCookie
            break;
        }
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
    Valid cookie name characters per RFC 6265 (token characters).
    Allows: A-Z, a-z, 0-9, and special characters: ! # $ % & ' * + - . ^ _ ` | ~
 */
static bool isValidCookieName(cchar *name)
{
    return sspn(name, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!#$%&'*+-.^_`|~") == slen(name);
}

/*
    Valid cookie value characters per RFC 6265 (cookie-value).
    Allows: A-Z, a-z, 0-9, and safe special characters excluding control chars, whitespace, quotes, comma, semicolon,
       and backslash.
    Safe subset: ! # $ % & ' ( ) * + - . / : = ? @ [ ] ^ _ ` { | } ~
 */
static bool isValidCookieValue(cchar *value)
{
    return sspn(value,
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!#$%&'()*+-./:=?@[]^_`{|}~") == slen(
        value);
}

/*
    RFC 6265 path-value: any CHAR except CTLs and ";"
 */
static bool isValidCookiePath(cchar *path)
{
    cuchar *cp;

    for (cp = (cuchar*) path; *cp; cp++) {
        if (*cp < 0x20 || *cp >= 0x7f || *cp == ';') {
            return 0;
        }
    }
    return 1;
}

/*
    Set a response cookie
 */
PUBLIC int webSetCookie(Web *web, cchar *name, cchar *value, cchar *path, Ticks lifespan, int flags)
{
    WebHost *host;
    cchar   *httpOnly, *secure, *sameSite;
    Ticks   maxAge;
    bool    remove;

    /*
        A null value means remove the cookie. That has to be an expiry the client acts on: a
        lifespan of zero otherwise selects the configured session timeout below, which stored an
        empty cookie for the whole session lifetime instead of deleting it.
     */
    remove = value == NULL;
    if (remove) {
        value = "";
    }
    if (slen(name) > 4096) {
        return R_ERR_WONT_FIT;
    }
    if (slen(value) > 4096) {
        return R_ERR_WONT_FIT;
    }
    if (!isValidCookieName(name)) {
        return R_ERR_BAD_ARGS;
    }
    if (!isValidCookieValue(value)) {
        return R_ERR_BAD_ARGS;
    }
    if (path == 0) {
        path = "/";
    }
    if (slen(path) > 4096) {
        return R_ERR_WONT_FIT;
    }
    if (!isValidCookiePath(path)) {
        return R_ERR_BAD_ARGS;
    }
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
    /*
        Max-Age is a Ticks (int64). It must be passed as a long long with a %lld conversion.
        A %d conversion would consume only one varargs slot on 32-bit ABIs and shift every
        pointer argument that follows.
     */
    if (remove) {
        //  Expires as well as Max-Age: clients predating RFC 6265 honour only the former
        webAddHeader(web, "Set-Cookie",
                     "%s=; Max-Age=0; Expires=Thu, 01 Jan 1970 00:00:00 GMT; path=%s; %s%sSameSite=%s",
                     name, path, secure, httpOnly, sameSite);
    } else {
        maxAge = (lifespan ? lifespan : host->sessionTimeout) / TPS;
        webAddHeader(web, "Set-Cookie", "%s=%s; Max-Age=%lld; path=%s; %s%sSameSite=%s",
                     name, value, (long long) maxAge, path, secure, httpOnly, sameSite);
    }
    return 0;
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/sockets.c ************/

/*
    sockets.c - WebSockets

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "web.h"

#if ME_COM_WEBSOCK
/*********************************** Locals ***********************************/

//  Length of a Sec-WebSocket-Key: RFC 6455 section 4.1 makes it base64 of a 16 byte nonce
#define WS_KEY_LEN 24

/********************************** Forwards **********************************/

static int addHeaders(Web *web);
static bool checkOrigin(Web *web);
static int selectProtocol(Web *web, cchar *protocol);

/*********************************** Code *************************************/

PUBLIC int webSocketOpen(WebHost *host)
{
    (void) host;

    assert(host);
    return 0;
}

PUBLIC void webSocketClose(WebHost *host)
{
    (void) host;

    assert(host);
}

/*
    Validate the Origin of a WebSocket upgrade.

    The WebSocket handshake is deliberately exempt from the same-origin policy - RFC 6455 4.2.2
    puts the check on the server, because a browser will open a cross-origin WebSocket and send
    the target's cookies with the handshake. Without this check, any page the victim visits can
    open a socket to the device and inherit their authenticated session, and unlike a cross-origin
    fetch there is no preflight and no response header that stops it.

    Policy:
      - Origin present: it must match the request Host, or appear in web.webSockets.origins.
        Configuring "*" opts out explicitly.
      - Origin absent: allowed. Only a browser attaches Origin automatically, so an absent Origin
        is not the browser-mediated attack this defends against - and a non-browser attacker can
        send any Origin it likes, so refusing the absent case costs legitimate non-browser clients
        (which device deployments commonly have) and buys nothing.

    Host is the right reference for the same-origin case: a browser sets it to the target it is
    connecting to and the attacking page cannot forge it, so Origin == Host is exactly "the page
    that opened this socket was served by us".
 */
static bool checkOrigin(Web *web)
{
    if (webMatchOrigin(web, web->host->webSocketsOrigins)) {
        return 1;
    }
    rError("web", "WebSocket upgrade refused: Origin %s is not the same origin and is not in "
           "web.webSockets.origins", web->origin);
    return 0;
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

    /* Reject body-framed handshake bytes; an intermediary already treats them as HTTP, not WebSocket frames. */
    if (web->rxLen > 0 || web->chunked) {
        webError(web, 400, "WebSocket upgrade request must not have a body");
        return R_ERR_BAD_ARGS;
    }
    if (!checkOrigin(web)) {
        webError(web, 403, "Cross-origin WebSocket upgrade refused");
        return R_ERR_BAD_ARGS;
    }
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
    webSocketSetLimits(ws, (size_t) host->webSocketsMaxFrame, (size_t) host->webSocketsMaxMessage);

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
    /*
        RFC 6455 section 4.1: the key is the base64 encoding of a 16 byte nonce, so exactly 24
        base64 characters. Validating it here keeps an attacker-length value away from the join
        below: sjoinbuf refuses rather than truncates and leaves the destination untouched, so an
        unchecked join left the SHA-1 running over uninitialised stack and encoded the digest of it
        into the response.
     */
    if ((key = webGetHeader(web, "sec-websocket-key")) == 0 || slen(key) != WS_KEY_LEN ||
        sspn(key, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=") != WS_KEY_LEN) {
        webError(web, 400, "Bad Sec-WebSocket-Key");
        return R_ERR_BAD_ARGS;
    }
    if (sjoinbuf(keybuf, sizeof(keybuf), key, WS_MAGIC) < 0) {
        webError(web, 400, "Bad Sec-WebSocket-Key");
        return R_ERR_BAD_ARGS;
    }
    webSetStatus(web, 101);
    webAddHeaderStaticString(web, "Upgrade", "WebSocket");
    webAddHeaderDynamicString(web, "Sec-WebSocket-Accept", cryptGetSha1Base64(keybuf, 0));

    protocol = webSocketGetProtocol(web->webSocket);
    if (protocol && *protocol) {
        webAddHeaderStaticString(web, "Sec-WebSocket-Protocol", protocol);
    }
    webAddHeader(web, "X-Request-Timeout", "%lld", (long long) (web->host->requestTimeout / TPS));
    webAddHeader(web, "X-Inactivity-Timeout", "%lld", (long long) (web->host->inactivityTimeout / TPS));
    webFinalize(web);
    return 0;
}

#endif /* ME_COM_WEBSOCK */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/test.c ************/

/*
    test.c - Test routines for debug mode only

    Should NEVER be included in a production build

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/


#if ME_WEB_TEST_ACTIONS || ME_BENCHMARK
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
    size_t   len;
    bool     isPrintable;
    int      i;

    json = jsonAlloc();
    jsonSetFmt(json, 0, "url", "%s", web->url);
    jsonSetFmt(json, 0, "method", "%s", web->method);
    jsonSetFmt(json, 0, "protocol", "%s", web->protocol);
    jsonSetFmt(json, 0, "connection", "%lld", (long long) web->conn);
    jsonSetFmt(json, 0, "count", "%lld", (long long) web->count);

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
        jsonSetFmt(json, 0, "bodyLength", "%lld", (long long) len);
        isPrintable = 1;
        for (i = 0; i < (int) len; i++) {
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
    WebSession *session;
    char       ipbuf[256];
    int        port;

    jsonSetFmt(json, 0, "authenticated", "%s", web->authChecked ? "authenticated" : "public");
    if (web->contentDisposition) {
        jsonSetFmt(json, 0, "contentDisposition", "%s", web->contentDisposition);
    }
    if (web->chunked) {
        jsonSetFmt(json, 0, "contentLength", "%s", "chunked");
    } else {
        jsonSetFmt(json, 0, "contentLength", "%lld", (long long) web->rxLen);
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
    //  Resolve rather than reading the borrowed web->session, which the owning table may have freed
    if ((session = webGetSession(web, 0)) != 0) {
        jsonSetFmt(json, 0, "session", "%s", session->id);
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
    jsonSetFmt(json, 0, "host.inactivityTimeout", "%lld", (long long) host->inactivityTimeout);
    jsonSetFmt(json, 0, "host.parseTimeout", "%lld", (long long) host->parseTimeout);
    jsonSetFmt(json, 0, "host.requestTimeout", "%lld", (long long) host->requestTimeout);
    jsonSetFmt(json, 0, "host.sessionTimeout", "%lld", (long long) host->sessionTimeout);
    jsonSetFmt(json, 0, "host.connections", "%d", host->connections);
    jsonSetFmt(json, 0, "host.maxBody", "%d", host->maxBody);
    jsonSetFmt(json, 0, "host.maxConnections", "%d", host->maxConnections);
    jsonSetFmt(json, 0, "host.maxHeader", "%d", host->maxHeader);
    jsonSetFmt(json, 0, "host.maxSessions", "%d", host->maxSessions);
    jsonSetFmt(json, 0, "host.maxUpload", "%d", host->maxUpload);
}

/*
    SSE test
 */
static void eventAction(Web *web)
{
    cchar *data;
    int64 id;
    int   count, i;

    //  "data" sends one event carrying that text, named by "name"
    if ((data = webGetQueryVar(web, "data", 0)) != 0) {
        webWriteEvent(web, 0, webGetQueryVar(web, "name", "test"), "%s", data);
        webFinalize(web);
        return;
    }

    /*
        "id" selects an explicit starting event id. This exercises the int64 id conversion
        in webWriteEvent, where a narrow conversion would shift the two string arguments
        that follow it.
     */
    id = stoi(webGetQueryVar(web, "id", "0"));
    count = (int) stoi(webGetQueryVar(web, "count", "100"));

    for (i = 0; i < count; i++) {
        webWriteEvent(web, id ? id + i : 0, "test", "Event %d", i);
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
    webWriteResponseString(web, 200, "error\n");
}

static void bulkOutput(Web *web)
{
    ssize count, i;

    count = stoi(webGetVar(web, "count", "100"));
    for (i = 0; i < count; i++) {
        webWriteFmt(web, "Hello World %010d\n", (int) i);
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
    webWriteResponseString(web, 200, "success\n");
}

/*
    Echo the length of the request body
 */
static void putAction(Web *web)
{
    char  buf[ME_BUFSIZE];
    ssize nbytes, total;

    total = 0;
    while ((nbytes = webRead(web, buf, sizeof(buf))) > 0) {
        total += nbytes;
    }
    webWriteResponse(web, 200, "%lld\n", (long long) total);
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
    rFree(web->role);
    web->role = sclone("user");
    web->authChecked = 1;
    rFree(web->username);
    web->username = sclone("user");

    if (web->vars && web->vars->count > 0) {
        webWriteValidatedJson(web, web->vars, NULL);
    } else if (web->qvars) {
        //  Echo the query the same way, so the request-direction query filter is observable
        webWriteValidatedJson(web, web->qvars, NULL);
    } else {
        webWriteValidatedData(web, rBufToString(web->body), NULL);
    }
    webFinalize(web);
}

#if ME_WEB_UPLOAD
/*
    Test upload action - assumes a ./tmp directory exists
    Not used by benchmark which tests the raw upload filter.
 */
static void uploadAction(Web *web)
{
    WebUpload *file;
    RName     *up;
    char      *path;

    if (web->uploads) {
        for (ITERATE_NAME_DATA(web->uploads, up, file)) {
            path = rJoinFile("./tmp", file->clientFilename);
            if (rCopyFile(file->filename, path, 0600) < 0) {
                webError(web, 500, "Cannot open output upload filename");
                rFree(path);
                break;
            }
            rFree(path);
        }
    }
    showRequest(web);
    webSetStatus(web, 200);
    webFinalize(web);
}
#endif

static void cookieAction(Web *web)
{
    cchar *name, *path, *value;
    Ticks lifespan;

    name = webGetQueryVar(web, "name", 0);
    value = webGetQueryVar(web, "value", 0);
    path = webGetQueryVar(web, "path", "/path");

    //  Lifespan is supplied in seconds. Zero selects the configured session timeout.
    lifespan = (Ticks) stoi(webGetQueryVar(web, "lifespan", "0")) * TPS;

    if (!name || !value || !path) {
        webError(web, 400, "Missing name or value");
        return;
    }
    if (webSetCookie(web, name, value, path, lifespan, 0) < 0) {
        webError(web, 404, "Invalid cookie");
        return;
    }
    webWriteResponseString(web, 200, "success");
}


static void sessionAction(Web *web)
{
    cchar *sessionToken, *name, *value;
    char  *token;

    if (smatch(web->path, "/test/session/create")) {
        /*
            Set a token in the session state and return it to the client.
            It will send it back in the query string to the /check action below.
         */
        token = cryptID(32);
        webSetSessionVar(web, "token", "%s", token);
        webWriteFmt(web, "%s", token);
        rFree(token);

    } else if (smatch(web->path, "/test/session/setvar")) {
        /*
            Store a caller-supplied value in a session variable and echo it back.
            Used to verify the value is never interpreted as a printf format string.
         */
        name = webGetQueryVar(web, "name", "var");
        value = webGetQueryVar(web, "value", "");
        webSetSessionVar(web, name, "%s", value);
        webWriteFmt(web, "%s", webGetSessionVar(web, name, ""));

    } else if (smatch(web->path, "/test/session/getvar")) {
        name = webGetQueryVar(web, "name", "var");
        webWriteFmt(web, "%s", webGetSessionVar(web, name, ""));

    } else if (smatch(web->path, "/test/session/login")) {
        /*
            Log in a caller-supplied username. The username must never be used as a format string.
         */
        name = webGetQueryVar(web, "user", "");
        value = webGetQueryVar(web, "role", "user");
        if (webLogin(web, name, value)) {
            webWriteFmt(web, "%s", web->username);
        } else {
            webWriteFmt(web, "denied");
        }

    } else if (smatch(web->path, "/test/session/logout")) {
        webLogout(web);
        webWriteFmt(web, "%s", "out");

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
    NOTE: this does not work in the Xcode debugger because the signal is intercepted by
    the Mach layer first.
 */
static void recurse(Web *web, int depth)
{
    volatile char buf[1024];

    //  Read the buffer back so the frame is really used and cannot be optimized away
    buf[0] = 'a';
    if (depth > 0 && buf[0] == 'a') {
        recurse(web, depth - 1);
    }
}

static void recurseAction(Web *web)
{
    // Recurse 1MB
    recurse(web, 1000);
    webWriteFmt(web, "Recursion complete");
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
    webWriteFmt(web, "{length: %lld}", (long long) total);
    webFinalize(web);
}

#if ME_COM_WEBSOCK
/*
    Echo back user provided message.
    On connected, buf will be null

    The WebSocket layer delivers each frame as it arrives: WS_EVENT_PARTIAL_MESSAGE for every frame
    but the last, and WS_EVENT_MESSAGE for the last, each carrying only that frame's own bytes. It
    does not reassemble. Echoing the payload of WS_EVENT_MESSAGE alone therefore returns just the
    final fragment of a fragmented message, which is what the Autobahn suite reported for 30 of its
    cases. Fragments are accumulated here so the echo is the whole message. The accumulator is
    bounded by the connection's maxMessage, which the WebSocket layer enforces on the way in.
 */
static void onEvent(WebSocket *ws, int event, cchar *buf, size_t len, Web *web)
{
    RBuf *frags;

    (void) web;

    frags = webSocketGetData(ws);

    if (event == WS_EVENT_PARTIAL_MESSAGE) {
        if (!frags) {
            frags = rAllocBuf(len + 1);
            webSocketSetData(ws, frags);
        }
        rPutBlockToBuf(frags, buf, len);

    } else if (event == WS_EVENT_MESSAGE) {
        // rTrace("test", "Echoing: %ld bytes", len);
        // Echo back with the same message type (preserves binary/text)
        // ws->type is the message type, which a continuation frame does not carry
        if (frags) {
            rPutBlockToBuf(frags, buf, len);
            webSocketSendBlock(ws, ws->type, rGetBufStart(frags), rGetBufLength(frags));
            rFreeBuf(frags);
            webSocketSetData(ws, 0);
        } else {
            webSocketSendBlock(ws, ws->type, buf, len);
        }
    }
}

static void webSocketAction(Web *web)
{
    if (!web->upgraded) {
        webError(web, 400, "Connection not upgraded to WebSocket");
        return;
    }
    webSocketRun(web->webSocket, (WebSocketProc) onEvent, web, web->rx, web->host->inactivityTimeout);

    //  A connection that ends part way through a fragmented message leaves fragments accumulated
    rFreeBuf(webSocketGetData(web->webSocket));
    webSocketSetData(web->webSocket, 0);
    rDebug("test", "WebSocket closed");
}
#endif

#if ME_WEB_FIBER_BLOCKS
/*
    Crash action for testing fiber exception blocks.
    On Windows: dereference null pointer (VEH catches hardware exceptions)
    On Unix: use raise() (signal handlers catch signals)
 */
static void crashNullAction(Web *web)
{
    rTrace("test", "Trigger SIGSEGV");
#if ME_WIN_LIKE
    volatile int *ptr = NULL;
    *ptr = 42;  // Actual null dereference triggers VEH
#else
    raise(SIGSEGV);
#endif
    webWriteResponseString(web, 200, "should not reach here\n");
}

/*
    Crash action for testing fiber exception blocks.
    On Windows: actual divide by zero (VEH catches EXCEPTION_INT_DIVIDE_BY_ZERO)
    On Unix/ARM64: use raise() since integer division doesn't generate SIGFPE on ARM
 */
static void crashDivideAction(Web *web)
{
    rTrace("test", "Trigger SIGFPE");
#if ME_WIN_LIKE
    volatile int zero = 0;
    volatile int result = 42 / zero;  // Actual divide by zero triggers VEH on x86/x64
    webWriteResponse(web, 200, "should not reach here: %d\n", result);
#else
    raise(SIGFPE);
    webWriteResponseString(web, 200, "should not reach here\n");
#endif
}
#endif

PUBLIC void webTestInit(WebHost *host, cchar *prefix)
{
    char url[128];

    rInfo("test", "Built with development web/test.c for testing -- not for production (DO NOT DISTRIBUTE)");

    webAddAction(host, SFMT(url, "%s/event", prefix), eventAction, NULL);
    webAddAction(host, SFMT(url, "%s/form", prefix), formAction, NULL);
    webAddAction(host, SFMT(url, "%s/bulk", prefix), bulkOutput, NULL);
    webAddAction(host, SFMT(url, "%s/error", prefix), errorAction, NULL);
    webAddAction(host, SFMT(url, "%s/success", prefix), successAction, NULL);
    webAddAction(host, SFMT(url, "%s/bench", prefix), successAction, NULL);
    webAddAction(host, SFMT(url, "%s/put", prefix), putAction, NULL);
    webAddAction(host, SFMT(url, "%s/show", prefix), showAction, NULL);
    //  Registered under a role-bearing route so a rewrite into that namespace has something to reach
    webAddAction(host, SFMT(url, "%s/admin/show", prefix), showAction, NULL);
    //  The shorter prefix is registered first: dispatch must still select the longer one
    webAddAction(host, SFMT(url, "%s/shadow", prefix), successAction, NULL);
    webAddAction(host, SFMT(url, "%s/shadow/deep", prefix), showAction, NULL);
    webAddAction(host, SFMT(url, "%s/stream", prefix), streamAction, NULL);
#if ME_WEB_UPLOAD
    webAddAction(host, SFMT(url, "%s/upload", prefix), uploadAction, NULL);
#endif
#if ME_COM_WEBSOCK
    webAddAction(host, SFMT(url, "%s/ws", prefix), webSocketAction, NULL);
#endif
    webAddAction(host, SFMT(url, "%s/session", prefix), sessionAction, NULL);
    webAddAction(host, SFMT(url, "%s/cookie", prefix), cookieAction, NULL);
    webAddAction(host, SFMT(url, "%s/xsrf", prefix), xsrfAction, NULL);
    webAddAction(host, SFMT(url, "%s/sig", prefix), sigAction, NULL);
    webAddAction(host, SFMT(url, "%s/buffer", prefix), bufferAction, NULL);
    webAddAction(host, SFMT(url, "%s/recurse", prefix), recurseAction, NULL);
#if ME_WEB_FIBER_BLOCKS
    webAddAction(host, SFMT(url, "%s/crash/null", prefix), crashNullAction, NULL);
    webAddAction(host, SFMT(url, "%s/crash/divide", prefix), crashDivideAction, NULL);
#endif
}

#else
PUBLIC void dummyTest(void)
{
}
#endif

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file src/upload.c ************/

/*
    upload.c -- File upload handler

    Example multipart/form-data request:

    POST /upload HTTP/1.1
    Host: example.com
    Content-Type: multipart/form-data; boundary=BOUNDARY123
    Content-Length: xxx

    --BOUNDARY123\r\n
    Content-Disposition: form-data; name="file"; filename="weather.txt"\r\n
    Content-Type: text/plain\r\n
    \r\n
    Cloudy and Rainy\r\n
    --BOUNDARY123--\r\n

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/*********************************** Includes *********************************/

#include    "web.h"

/*********************************** Forwards *********************************/
#if ME_WEB_UPLOAD

static WebUpload *allocUpload(Web *web, cchar *name, cchar *filename);
static void freeUpload(WebUpload *up);
static int createUploadFile(cchar *dir, char **filename);
static char *findBoundary(cchar *contentType);
static size_t getUploadDataLength(Web *web);
static int processUploadData(Web *web);
static int processUploadHeaders(Web *web);

/************************************* Code ***********************************/

/*
    Parse a possibly quoted boundary parameter (RFC 2046 5.1.1). Return an allocated value or NULL.
 */
static char *findBoundary(cchar *contentType)
{
    char *params, *param, *tok, *value, *end, *result;

    result = 0;
    params = sclone(contentType);
    //  The first token is the media type. Boundary characters do not include ";".
    for (param = stok(params, ";", &tok); param; param = stok(NULL, ";", &tok)) {
        param = strim(param, " \t", R_TRIM_BOTH);
        if (sncaselesscmp(param, "boundary=", 9) == 0) {
            value = param + 9;
            if (*value == '"') {
                value++;
                if ((end = strchr(value, '"')) == 0 || end[1]) {
                    break;
                }
                *end = '\0';
            }
            if (*value) {
                result = sclone(value);
            }
            break;
        }
    }
    rFree(params);
    return result;
}

PUBLIC int webInitUpload(Web *web)
{
    char *boundary;

    if ((boundary = findBoundary(web->contentType)) == 0) {
        webError(web, 400, "Bad boundary");
        return R_ERR_BAD_ARGS;
    }
    /*
        Check the 70-character boundary limit before allocating upload state (RFC 2046 5.1.1). Oversized boundaries
        cannot fit the part scanner.
     */
    if (slen(boundary) > WEB_MAX_BOUNDARY) {
        rFree(boundary);
        webError(web, 400, "Upload boundary is too long");
        return R_ERR_BAD_ARGS;
    }
    web->boundary = sjoin("--", boundary, NULL);
    rFree(boundary);
    web->boundaryLen = slen(web->boundary);
    web->uploads = rAllocHash(0, 0);
    web->uploadList = rAllocList(0, 0);
    web->numUploads = 0;
    web->uploadSize = 0;
    if (!web->vars) {
        web->vars = jsonAlloc();
    }
    return 0;
}

PUBLIC void webFreeUpload(Web *web)
{
    WebUpload *upload;
    int       next;

    //  The boundary is set before web->uploads, so free it whether or not the upload started
    rFree(web->boundary);
    web->boundary = 0;
    if (web->uploads == 0) return;

    /*
        Free from the list, which owns every upload including any that a duplicate part name
        displaced from the hash.
     */
    for (ITERATE_ITEMS(web->uploadList, upload, next)) {
        freeUpload(upload);
    }
    rFreeList(web->uploadList);
    web->uploadList = 0;
    rFreeHash(web->uploads);
    web->uploads = 0;
    rFree(web->uploadName);
    web->uploadName = 0;
    rFree(web->uploadContentType);
    web->uploadContentType = 0;
}

/*
    Allocate a new upload after validating the filename. This also opens the file.
 */
static WebUpload *allocUpload(Web *web, cchar *name, cchar *clientFilename)
{
    WebUpload *upload;
    char      *filename, *path;
    int       fd;

    /*
        Count files here rather than parts in the boundary loop: this is the only path that opens
        a descriptor and consumes an upload-directory inode, so it is the resource the limit is
        for. Non-file parts are bounded separately by web.limits.vars via webAddRequestVar.
        A configured zero disables the limit.
     */
    if (web->host->maxUploads > 0 && ++web->numUploads > web->host->maxUploads) {
        webError(web, 413, "Too many files uploaded");
        return 0;
    }
    if (!name || *name == '\0') {
        webError(web, -400, "Missing upload name for filename");
        return 0;
    }
    if (!clientFilename || *clientFilename == '\0') {
        webError(web, -400, "Missing upload client filename");
        return 0;
    }
    if ((path = webNormalizePath(clientFilename)) == 0) {
        webError(web, -400, "Bad upload client filename");
        return 0;
    }
    // Enhanced validation against directory traversal
    if (*path == '.' || scontains(path, "..") || strpbrk(path, "\\/:*?<>|~\"'%`^\n\r\t\f")) {
        webError(web, -400, "Bad upload client filename");
        rFree(path);
        return 0;
    }
    // Check for URL-encoded directory traversal attempts
    if (scontains(path, "%2e") || scontains(path, "%2E") || scontains(path, "%2f") ||
        scontains(path, "%2F") || scontains(path, "%5c") || scontains(path, "%5C")) {
        webError(web, -400, "Bad upload client filename");
        rFree(path);
        return 0;
    }
    /*
        Create the file to hold the uploaded data
     */
    if ((fd = createUploadFile(web->host->uploadDir, &filename)) < 0) {
        webError(web, 500, "Cannot create upload temp file. Check upload directory configuration");
        rFree(path);
        return 0;
    }
    rTrace("web", "File upload of: %s stored as %s", path, filename);

    if ((upload = rAllocType(WebUpload)) == 0) {
        // Memory errors handled globally
        webError(web, 500, "Cannot allocate upload");
        close(fd);
        unlink(filename);
        rFree(filename);
        rFree(path);
        return 0;
    }
    upload->name = sclone(name);
    upload->clientFilename = path;
    upload->filename = filename;
    upload->fd = fd;
    upload->size = 0;

    return upload;
}

/*
    Keep the descriptor from exclusive creation. Never reopen a temporary pathname: even an
    O_NOFOLLOW reopen would still permit replacement by a regular file or a hard link.
    Local to web until the companion r temporary-file descriptor API is available.
 */
static int createUploadFile(cchar *dir, char **filename)
{
    static cchar hex[] = "0123456789abcdef";
    uchar        random[16];
    char         token[33], *path;
    int          attempt, error, fd, i;

    *filename = NULL;
    for (attempt = 0; attempt < 16; attempt++) {
        if (cryptGetRandomBytes(random, sizeof(random), 0) < 0) {
            return -1;
        }
        for (i = 0; i < 16; i++) {
            token[i * 2] = hex[random[i] >> 4];
            token[i * 2 + 1] = hex[random[i] & 0xf];
        }
        token[32] = '\0';
        if ((path = sjoin(dir, "/tmp-", token, ".tmp", NULL)) == NULL) {
            return -1;
        }
        fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_BINARY, 0600);
        if (fd >= 0) {
            *filename = path;
            return fd;
        }
        error = errno;
        rFree(path);
        if (error != EEXIST) {
            return -1;
        }
    }
    return -1;
}

static void freeUpload(WebUpload *upload)
{
    /* Close before unlinking; Windows _open does not allow deletion of open files. */
    if (upload->fd >= 0) {
        close(upload->fd);
        upload->fd = -1;
    }
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
    char  suffix[8];
    ssize nbytes;

    while (1) {
        if ((nbytes = webBufferUntil(web, web->boundary, WEB_BUF_BOOST_2X)) <= 0) {
            return webError(web, -400, "Bad upload request boundary");
        }
        webConsumeBody(web, (size_t) nbytes);

        //  Should be \r\n after each boundary. On the last boundary, it is "--\r\n"
        suffix[0] = '\0';
        if (webReadUntil(web, "\r\n", suffix, sizeof(suffix)) < 0) {
            return webError(web, -400, "Bad upload request suffix");
        }
        if (sncmp(suffix, "--\r\n", 4) == 0) {
            // Final boundary
            break;
        }
        // Middle boundary
        if (sncmp(suffix, "\r\n", 2) != 0) {
            return webError(web, -400, "Bad upload request trailer");
        }
        if (processUploadHeaders(web) < 0) {
            return R_ERR_CANT_COMPLETE;
        }
        if (processUploadData(web) < 0) {
            return R_ERR_CANT_WRITE;
        }
    }
    /*
        A framed body is drained to its end when the request completes. A body with neither a
        Content-Length nor a Transfer-Encoding has no end to read to, so it ends at the final
        boundary.
     */
    if (web->rxLen < 0 && !web->chunked) {
        web->rxRemaining = 0;
    }
    return 0;
}

static int processUploadHeaders(Web *web)
{
    WebUpload *upload;
    char      *content, *field, *filename, *key, *next, *rest, *value, *type;
    ssize     nbytes;

    if ((nbytes = webBufferUntil(web, "\r\n\r\n", WEB_BUF_BOOST_2X)) < 2) {
        webError(web, -400, "Bad upload headers");
        return R_ERR_BAD_REQUEST;
    }
    content = web->rx->start;
    content[nbytes - 2] = '\0';
    webConsumeBody(web, (size_t) nbytes);

    if (web->host->flags & WEB_SHOW_REQ_HEADERS) {
        rLog("raw", "web", "Upload Header %d <<<<\n\n%s\n", (int) web->rx->buflen, content);
    }

    //  Each part stands alone: nothing from the previous part's headers carries over
    web->upload = 0;
    rFree(web->uploadName);
    web->uploadName = 0;
    filename = type = 0;

    /* Allocate each upload only after its complete part header. Require CRLF and reject bare CR/LF. */
    for (key = content; key && *key; key = next) {
        if ((next = strstr(key, "\r\n")) != 0) {
            *next = '\0';
            next += 2;
        }
        if (strpbrk(key, "\r\n")) {
            webError(web, -400, "Bad upload headers");
            return R_ERR_BAD_REQUEST;
        }
        ssplit(key, ": ", &rest);
        if (scaselessmatch(key, "Content-Disposition")) {
            for (field = rest; field && stok(field, ";", &rest); ) {
                field = strim(field, " ", R_TRIM_BOTH);
                field = ssplit(field, "=", &value);
                value = strim(value, "\"", R_TRIM_BOTH);

                if (scaselessmatch(field, "form-data")) {
                    // Nothing to do

                } else if (scaselessmatch(field, "name")) {
                    rFree(web->uploadName);
                    web->uploadName = sclone(value);

                } else if (scaselessmatch(field, "filename")) {
                    if (filename) {
                        webError(web, -400, "Bad upload headers. Multiple filenames in one part");
                        return R_ERR_BAD_REQUEST;
                    }
                    filename = value;
                }
                field = rest;
            }
        } else if (scaselessmatch(key, "Content-Type")) {
            type = strim(rest, " ", R_TRIM_BOTH);
        }
    }
    if (!web->uploadName) {
        webError(web, -400, "Bad upload headers. Missing Content-Disposition name");
        return R_ERR_BAD_REQUEST;
    }
    if (filename) {
        if ((upload = allocUpload(web, web->uploadName, filename)) == 0) {
            return R_ERR_CANT_COMPLETE;
        }
        upload->contentType = type ? sclone(type) : 0;
        /*
            Keep every upload in an ownership list; duplicate names retain last-wins hash lookup without orphaning
            earlier files.
         */
        rAddItem(web->uploadList, upload);
        rAddName(web->uploads, upload->name, upload, 0);
        web->upload = upload;
    }
    return 0;
}

/*
    Process upload file and form data
    If file data, the entire file between boundaries is read and saved
    If form data, web vars are defined for each form field
 */
static int processUploadData(Web *web)
{
    WebUpload *upload;
    RBuf      *buf;
    char      *data;
    size_t    buffered, len, prior;
    ssize     nbytes, written;

    upload = web->upload;
    buf = web->rx;
    prior = (size_t) -1;
    do {
        len = 0;
        if ((nbytes = webBufferUntil(web, web->boundary, WEB_BUF_BOOST_16X)) < 0) {
            return webError(web, -400, "Bad upload request boundary");
        }
        if (upload && upload->fd >= 0) {
            /*
                If webBufferUntil returned 0 (short), then a complete boundary was not seen. In this case,
                write the data and continue but preserve a possible partial boundary with \r\n delimiter.
             */
            if (nbytes) {
                /*
                    Extract the data before the \r\n delimiter and boundary. Do the subtraction in
                    signed space and clamp at zero BEFORE casting to size_t: casting nbytes to size_t
                    first makes `(size_t) nbytes - boundaryLen - 2` underflow to a huge value when a
                    boundary matches with fewer than boundaryLen + 2 preceding bytes, which the
                    maxUpload guard below then wraps past, driving an out-of-bounds read in write().
                 */
                len = (size_t) max(0, nbytes - (ssize) web->boundaryLen - 2);
            } else {
                /*
                    Not a complete boundary, so preserve a possible partial boundary.
                 */
                len = getUploadDataLength(web);
            }
            if (len > 0) {
                /* Check total bytes across all files, including requests without a declared length. */
                if ((web->uploadSize + len) > (size_t) web->host->maxUpload) {
                    if (upload->fd >= 0) {
                        close(upload->fd);
                        upload->fd = -1;
                    }
                    return webError(web, 413, "Uploaded files exceed maximum %d", web->host->maxUpload);
                }
                if ((written = write(upload->fd, buf->start, (uint) len)) != (ssize) len) {
                    if (upload->fd >= 0) {
                        close(upload->fd);
                        upload->fd = -1;
                    }
                    return webError(web, 500, "Cannot write uploaded file");
                }
                webConsumeBody(web, len);
                upload->size += (size_t) written;
                web->uploadSize += (size_t) written;
                if (upload->fd >= 0 && web->host->flags & WEB_SHOW_REQ_HEADERS) {
                    rLog("raw", "web", "Upload File Data %d <<<<\n%d bytes\n", (int) written, (int) upload->size);
                }
            }

        } else {
            if (nbytes == 0) {
                return webError(web, 414, "Uploaded form header is too big");
            }
            //  Strip \r\n. Keep boundary in data to be consumed by webProcessUpload.
            nbytes -= (ssize) web->boundaryLen;
            //  The value and its CRLF. An empty value is legal: browsers send empty fields.
            if (nbytes < 2) {
                return webError(web, -400, "Bad upload form data");
            }
            buf->start[nbytes - 2] = '\0';
            data = webDecode(buf->start);
            if (webAddRequestVar(web, web->vars, web->uploadName, data) < 0) {
                return R_ERR_BAD_REQUEST;
            }
            if (web->host->flags & WEB_SHOW_REQ_HEADERS) {
                rLog("raw", "web", "Upload Form Field <<<<\n\n%s = %s\n", web->uploadName, data);
            }
            webConsumeBody(web, (size_t) nbytes);
        }
        if (nbytes == 0 && len == 0) {
            /*
                No complete boundary was seen and no data could be written, so this pass consumed
                nothing. That is normal while a partial boundary waits for more bytes, but only if
                more bytes are arriving: webBufferUntil returns zero without reading once the
                buffered body reaches its scan limit, so a loop that sees the same amount of data
                twice will never progress and must stop rather than spin the fiber.
             */
            buffered = webBufferedBody(web);
            if (buffered == prior) {
                return webError(web, -400, "Upload is not making progress");
            }
            prior = buffered;
        }
    } while (nbytes == 0);

    if (upload && upload->fd >= 0) {
        close(upload->fd);
        upload->fd = -1;
    }
    return 0;
}

/*
    Get the maximum amount of user data that can be read from the buffer.
    This is the amount of data that can be read without reading past the boundary.
 */
static size_t getUploadDataLength(Web *web)
{
    ssize avail;
    char  *start, *end, *cp, *from;

    start = web->rx->start;
    end = &start[webBufferedBody(web)];
    from = max(start, end - (ssize) web->boundaryLen - 2);

    /*
        Check if the first character of the boundary "-" is found at the end of the body data
        Start search at the end of the data, less the boundary length and \r\n delimiter
     */
    if ((cp = memchr(from, web->boundary[0], (size_t) (end - from))) != NULL) {
        //  Signed before clamping - max(0, <unsigned>) cannot clamp a value that already wrapped
        avail = (ssize) (cp - start) - 2;
        return avail > 0 ? (size_t) avail : 0;
    }
    avail = (ssize) (end - start) - 2;
    return avail > 0 ? (size_t) avail : 0;
}
#endif /* ME_WEB_UPLOAD */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
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
    { 411, "Length Required" },
    { 413, "Request too large" },
    { 421, "Misdirected Request" },
    { 429, "Too Many Requests" },
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

/*
    HTTP date is always 29 characters long
    Format as RFC 7231 IMF-fixdate: "Mon, 10 Nov 2025 21:28:28 GMT"
    Caller must free
 */
PUBLIC char *webHttpDate(time_t when)
{
    static cchar *days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static cchar *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    struct tm    tm;
    char         buf[32];

#if ME_WIN_LIKE
    if (gmtime_s(&tm, &when) != 0) {
        return 0;
    }
#else
    if (gmtime_r(&when, &tm) == 0) {
        return 0;
    }
#endif
    /*
        Do not use strftime here. Its %a and %b are locale-dependent, and an embedding application
        that calls setlocale - a reasonable thing to do for a localised device UI - would silently
        change the wire format. RFC 9110 5.6.7 fixes HTTP-date to these English abbreviations
        regardless of locale.
     */
    if (tm.tm_wday < 0 || tm.tm_wday > 6 || tm.tm_mon < 0 || tm.tm_mon > 11) {
        return 0;
    }
    sfmtbuf(buf, sizeof(buf), "%s, %02d %s %d %02d:%02d:%02d GMT",
            days[tm.tm_wday], tm.tm_mday, months[tm.tm_mon], tm.tm_year + 1900,
            tm.tm_hour, tm.tm_min, tm.tm_sec);
    return sclone(buf);
}

PUBLIC cchar *webGetDocs(WebHost *host)
{
    return host->docs;
}

/*
    URL encoding decode
 */
/*
    Percent-decode in place.

    "plusAsSpace" selects the form. RFC 3986 3.3 makes "+" a literal character in a path; the
    plus-means-space rule belongs only to application/x-www-form-urlencoded, that is to query
    strings and form bodies. Decoding a path with that rule aliases two spellings onto one file
    - "/a+b.txt" and "/a%20b.txt" both reach "a b.txt" - which evades any route policy keyed on
    the literal path, exactly as case-variance did.
 */
static char *decode(char *str, bool plusAsSpace)
{
    char   *ip,  *op;
    size_t len;
    int    num, i, c;

    if (str == 0) {
        return 0;
    }
    len = slen(str);
    for (ip = op = str; len > 0 && *ip; ip++, op++) {
        if (*ip == '+' && plusAsSpace) {
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
/*
    Redact credential header values from a header block before it is logged
 */
PUBLIC char *webRedactHeaders(cchar *headers)
{
    static cchar *secrets[] = { "authorization", "proxy-authorization", "cookie", "set-cookie", 0 };
    RBuf         *buf;
    cchar        *colon, *eol, *line;
    size_t       nameLen;
    int          i;
    bool         redact;

    if (!headers) {
        return 0;
    }
    buf = rAllocBuf(slen(headers) + 1);
    for (line = headers; *line; line = eol) {
        eol = line + strcspn(line, "\n");
        if (*eol == '\n') {
            eol++;
        }
        redact = 0;
        nameLen = 0;
        if ((colon = memchr(line, ':', (size_t) (eol - line))) != 0) {
            nameLen = (size_t) (colon - line);
            for (i = 0; secrets[i]; i++) {
                if (nameLen == slen(secrets[i]) && sncaselesscmp(line, secrets[i], nameLen) == 0) {
                    redact = 1;
                    break;
                }
            }
        }
        if (redact) {
            rPutBlockToBuf(buf, line, nameLen + 1);
            rPutStringToBuf(buf, " [redacted]");
            if (eol > line && eol[-1] == '\n') {
                rPutStringToBuf(buf, (eol - 1 > line && eol[-2] == '\r') ? "\r\n" : "\n");
            }
        } else {
            rPutBlockToBuf(buf, line, (size_t) (eol - line));
        }
    }
    return rBufToStringAndFree(buf);
}

/*
    Decode a query or form value. "+" becomes a space.
 */
PUBLIC char *webDecode(char *str)
{
    return decode(str, 1);
}

/*
    Decode a URI path. "+" is a literal plus, per RFC 3986.
 */
PUBLIC char *webDecodePath(char *str)
{
    return decode(str, 0);
}

PUBLIC char *webParseUrl(cchar *uri,
                         cchar **scheme,
                         cchar **host,
                         int *port,
                         cchar **path,
                         cchar **query,
                         cchar **hash)
{
    char  *buf, *end, *next, *tok;
    int64 value;
    bool  hasAuthority;

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
        hasAuthority = 0;
        if (*tok == '[' && ((next = strchr(tok, ']')) != 0)) {
            /* IPv6  [::]:port/url */
            if (host) {
                *host = &tok[1];
            }
            *next++ = 0;
            /*
                The literal is terminated at its "]". Whatever follows is the ":port" and "/path",
                which the shared block below parses exactly as it does for a named host. Falling
                straight through to the path assignment instead left the port unset and glued the
                ":port" onto the front of the path.
             */
            tok = next;
            hasAuthority = 1;

        } else if (*tok && *tok != '/') {
            // hostname:port/path
            if (host) {
                *host = tok;
            }
            tok = spbrk(tok, ":/");
            hasAuthority = 1;
        }
        if (hasAuthority && tok) {
            if (*tok == ':') {
                *tok++ = 0;
                if (port) {
                    //  Strict: a sign, white space or a value that only fits after truncation is not a port
                    if (sparseUint(tok, 10, 65535, &end, &value) < 0 || (*end != '\0' && *end != '/') || value == 0) {
                        goto fail;
                    }
                    *port = (int) value;
                }
                if ((tok = schr(tok, '/')) == 0) {
                    tok = "";
                }
            } else if (*tok == '/') {
                //  Remove only the authority separator. A further leading "/" belongs to the path.
                *tok++ = 0;
                if (path) {
                    *path = tok;
                }
                tok = 0;
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

fail:
    if (scheme) *scheme = 0;
    if (host) *host = 0;
    if (port) *port = 0;
    if (path) *path = 0;
    if (query) *query = 0;
    if (hash) *hash = 0;
    rFree(buf);
    return 0;
}

/*
    Check if path needs normalization (contains //, /./, /../, or ends with /. or /..)
    Returns true if normalization is needed, false if path is already clean.
 */
static bool needsNormalization(cchar *path)
{
    cchar *cp;

    for (cp = path; *cp; cp++) {
        if (*cp == '/') {
            // Check for // (redundant separator)
            if (cp[1] == '/') {
                return true;
            }
            // Check for /. patterns
            if (cp[1] == '.') {
                // /. at end or /./ (current dir)
                if (cp[2] == '\0' || cp[2] == '/') {
                    return true;
                }
                // /.. at end or /../ (parent dir)
                if (cp[2] == '.' && (cp[3] == '\0' || cp[3] == '/')) {
                    return true;
                }
            }
        }
    }
    // Check for leading ./ or ..
    if (path[0] == '.') {
        if (path[1] == '\0' || path[1] == '/') {
            return true;
        }
        if (path[1] == '.' && (path[2] == '\0' || path[2] == '/')) {
            return true;
        }
    }
    return false;
}

/*
    Normalize a path to remove "./",  "../" and redundant separators.
    This does not map separators nor change case. Returns an allocated path, caller must free.

    SECURITY Acceptable:: This routine does not check for path traversal because
    all callers validate the path before calling this routine.
 */
PUBLIC char *webNormalizePath(cchar *pathArg)
{
    char   *path, *src, *dst, *mark;
    char   *localStack[64], **segments;
    size_t len, nseg, i, j, maxSeg;
    bool   isAbs, hasTrail, heapSegments;

    if (pathArg == 0 || *pathArg == '\0') {
        return 0;
    }
    len = slen(pathArg);
    isAbs = (pathArg[0] == '/');
    hasTrail = (len > 1 && pathArg[len - 1] == '/');

    // Fast path: if no normalization needed, just clone
    if (!needsNormalization(pathArg)) {
        return sclone(pathArg);
    }

    // Single allocation for path data
    path = sclone(pathArg);

    // Use stack array for small paths, heap for large
    maxSeg = (len / 2) + 2;
    if (maxSeg <= 64) {
        segments = localStack;
        heapSegments = false;
    } else {
        segments = rAlloc(sizeof(char*) * maxSeg);
        heapSegments = true;
    }

    // Split path into segments
    nseg = 0;
    for (mark = src = path; *src; src++) {
        if (*src == '/') {
            *src = '\0';
            if (mark != src) {
                segments[nseg++] = mark;
            }
            mark = src + 1;
        }
    }
    // Add final segment
    if (mark != src) {
        segments[nseg++] = mark;
    }

    // Process segments: skip ".", handle ".."
    j = 0;
    for (i = 0; i < nseg; i++) {
        char *sp = segments[i];
        // Check for "." (inline comparison)
        if (sp[0] == '.' && sp[1] == '\0') {
            continue;
        }
        // Check for ".." (inline comparison)
        if (sp[0] == '.' && sp[1] == '.' && sp[2] == '\0') {
            if (j > 0) {
                j--;
            } else {
                // Attempt to traverse above root - security violation
                rFree(path);
                if (heapSegments) {
                    rFree(segments);
                }
                return NULL;
            }
        } else {
            segments[j++] = sp;
        }
    }
    nseg = j;

    // Rebuild path in-place (output is always <= input)
    dst = path;
    if (isAbs) {
        *dst++ = '/';
    }
    for (i = 0; i < nseg; i++) {
        char   *sp = segments[i];
        size_t segLen = slen(sp);
        memmove(dst, sp, segLen);
        dst += segLen;
        if (i < nseg - 1) {
            *dst++ = '/';
        }
    }
    if (hasTrail && dst > path && dst[-1] != '/') {
        *dst++ = '/';
    }
    if (dst == path) {
        *dst++ = isAbs ? '/' : '.';
    }
    *dst = '\0';

    if (heapSegments) {
        rFree(segments);
    }
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
    size_t       len;

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
            *op++ = (char) c;
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
        rDebug("web", "Cannot parse json: %s", errorMsg);
        rFree(errorMsg);
        return 0;
    }
    return json;
}

/*
    Define a variable supplied by the client, from a query string, a url-encoded body or a multipart
    form field. Every producer of client variables must come through here so none can miss the count
    cap: capping only the url-encoded parser left the multipart producer unbounded.

    web.limits.vars caps the count over the query and the body together. Each jsonSet linear-scans the
    already-parsed keys, so parsing is O(n^2) in the variable count; with only the byte limits to bound
    it, a 100K form body of ~25k short pairs or a 20MB multipart body of ~100k form-field parts drives
    minutes of synchronous CPU before the handler runs - a cheap, unauthenticated exhaustion vector on
    a single-threaded runtime.
 */
PUBLIC int webAddRequestVar(Web *web, Json *vars, cchar *name, cchar *value)
{
    if (++web->numVars > web->host->maxVars) {
        webError(web, 413, "Too many request parameters (max %d)", web->host->maxVars);
        return R_ERR_WONT_FIT;
    }
    /*
        Store client names literally and reject structural characters that webGetVar cannot address. Structured
        input uses application/json.
     */
    if (name == NULL || strpbrk(name, ".[]") != NULL) {
        webError(web, 400, "Invalid request parameter name");
        return R_ERR_BAD_ARGS;
    }
    jsonSetKey(vars, 0, name, value, 0);
    return 0;
}

PUBLIC int webParseEncoded(Web *web, Json *vars, cchar *str)
{
    char *data, *keyword, *value, *tok;
    int  rc;

    rc = 0;
    data = sclone(str);
    keyword = stok(data, "&", &tok);
    while (keyword != NULL) {
        if ((value = strchr(keyword, '=')) != NULL) {
            *value++ = '\0';
            webDecode(value);
        } else {
            value = "";
        }
        webDecode(keyword);
        if ((rc = webAddRequestVar(web, vars, keyword, value)) < 0) {
            break;
        }
        keyword = stok(tok, "&", &tok);
    }
    rFree(data);
    return rc;
}

PUBLIC int webParseQuery(Web *web)
{
    if (web->query) {
        return webParseEncoded(web, web->qvars, web->query);
    }
    return 0;
}

PUBLIC int webParseForm(Web *web)
{
    return webParseEncoded(web, web->vars, rBufToString(web->body));
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
    Get a request query variable from the request URL.
 */
PUBLIC cchar *webGetQueryVar(Web *web, cchar *name, cchar *defaultValue)
{
    return jsonGet(web->qvars, 0, name, defaultValue);
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */


/********* Start of file src/validate.c ************/

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



/************************************ Forwards *********************************/

static cchar *getType(Web *web, int sid);
static int parseUrl(Web *web);
static bool valError(Web *web, Json *json, cchar *fmt, ...);
static bool validateArray(Web *web, RBuf *buf, Json *json, int jid, int sid, int depth, cchar *tag);
static int findField(Json *signatures, int fieldsId, cchar *name);
static bool validateObject(Web *web, RBuf *buf, Json *json, int jid, int sid, int depth, cchar *tag);
static bool validatePrimitive(Web *web, cchar *data, int sid, cchar *tag);
static bool validateProperty(Web *web, RBuf *buf, Json *json, int jid, int sid, cchar *tag);

/************************************* Code ***********************************/
/*
    Validate the request using the signature selected for the URL by validateRequest.
    This routine will generate an error response if the signature is not found or if
    the signature is invalid. Depending on the host->strictSignatures flag, it will either
    return 0 or R_ERR_BAD_REQUEST. If invalid, the connection is closed after the error
    response is written.
 */
PUBLIC bool webValidateRequest(Web *web)
{
    WebHost *host;
    cchar   *type;
    int     sid;

    host = web->host;
    if (!host->signatures) {
        return 0;
    }
    if (web->signature < 0) {
        /*
            Strict mode refuses a request the signature file does not describe. That is only a
            sensible demand of a route that asked to be validated: a route validated by default -
            static files, endpoints outside the signature file - simply has nothing to check.
         */
        if (host->strictSignatures && (web->route->validate == WEB_VALIDATE_ON ||
                                       (web->policy && web->policy->validate == WEB_VALIDATE_ON))) {
            return valError(web, NULL, "Missing request signature for request");
        }
        rDebug("web", "Cannot find signature for %s, continuing.", web->path);
        return 1;
    }
    //  Optional query signature. It is checked in addition to the body, not instead of it.
    if (web->qvars && (sid = jsonGetId(host->signatures, web->signature, "request.query")) >= 0) {
        if (!webValidateSignature(web, NULL, web->qvars, 0, sid, 0, "query")) {
            return 0;
        }
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
        if (!web->vars) {
            web->vars = jsonAlloc();
        }
        return webValidateSignature(web, NULL, web->vars, 0, sid, 0, "request");
    }
    return validatePrimitive(web, rBufToString(web->body), sid, "request");
}

/*
    Validate a JSON object against the API signature and write the validated JSON to a buffer.

    This evaluates the json properties starting at the "jid" node.
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
        rError("web", "Missing signatures or signature ID");
        return 0;
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
    int      iid, oid;

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
    /* Walk by index because filtering can change the JSON node count. */
    for (iid = jid + 1; iid < json->nodes[jid].last; iid = json->nodes[iid].last) {
        item = &json->nodes[iid];
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
            if (buf && jsonPutToBuf(buf, json, iid, JSON_JSON) < 0) {
                return valError(web, NULL, "Cannot serialize %s", tag);
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
/*
    Response filtering omits output; request filtering must remove vars/qvars. Return true after removal so the
    caller revisits the sibling moved into this slot.
 */
static bool omitField(RBuf *buf, Json *json, int vid)
{
    if (buf) {
        return 0;
    }
    jsonRemove(json, vid, 0);
    return 1;
}

/*
    Look up literal field names; interpreting client names as paths could select other signature nodes.
 */
static int findField(Json *signatures, int fieldsId, cchar *name)
{
    JsonNode *field;

    for (ITERATE_JSON_ID(signatures, fieldsId, field, fid)) {
        if (smatch(field->name, name)) {
            return fid;
        }
    }
    return -1;
}

static bool validateObject(Web *web, RBuf *buf, Json *json, int jid, int sid, int depth, cchar *tag)
{
    Json     *signatures;
    JsonNode *drop, *field, *fields, *var;
    cchar    *def, *dropRole, *ftype, *methodRole, *required, *role, *value;
    char     dropBuf[128];
    bool     hasRequired, hasWild, omitted, strict;
    int      fid, fieldsId, vid;

    signatures = web->host->signatures;
    strict = web->host->strictSignatures;

    /*
        Require an object; permit an absent body.
     */
    if (json && (var = jsonGetNode(json, jid, 0)) != 0 && var->type != JSON_OBJECT) {
        return valError(web, NULL, "Bad %s, expected an object", tag);
    }
    fields = jsonGetNode(signatures, sid, "fields");
    if (!fields) {
        //  Generic object with no fields defined
        if (buf && json && jsonPutToBuf(buf, json, jid, JSON_JSON) < 0) {
            return valError(web, NULL, "Cannot serialize %s", tag);
        }
        return 1;
    }
    hasWild = jsonGetBool(signatures, sid, "hasWild", 0);            // Allow any properties
    hasRequired = jsonGetBool(signatures, sid, "hasRequired", 0);    // Signature has required fields
    /*
        Determine the effective role requirement. The signature's declared role takes precedence,
        ensuring that signature-specific authorization is enforced even on public routes. If the
        signature omits a role, fall back to the route's role (if any).
     */
    methodRole = jsonGet(signatures, sid, "role", web->route->role);

    if (buf) {
        rPutCharToBuf(buf, '{');
    }
    if (hasRequired) {
        /*
            Ensure all required fields are present.
         */
        for (ITERATE_JSON(signatures, fields, field, requiredId)) {
            required = jsonGet(signatures, requiredId, "required", 0);
            if (required) {
                value = jsonGet(json, jid, field->name, 0);
                if (!value) {
                    def = jsonGet(signatures, requiredId, "default", 0);
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

    if (json && json->count > 0) {
        /*
            Revisit the current index after removal; nested filtering changes node counts and invalidates
            ITERATE_JSON bounds.
         */
        for (vid = jid + 1, omitted = 0; vid < json->nodes[jid].last;
             vid = omitted ? vid : json->nodes[vid].last, omitted = 0) {
            var = &json->nodes[vid];
            if (var->name[0] == '_' || smatch(var->name, "pk") || smatch(var->name, "sk")) {
                // Always hidden
                omitted = omitField(buf, json, vid);
                continue;
            }
            fid = findField(signatures, fieldsId, var->name);
            if (fid < 0) {
                if (!hasWild) {
                    if (strict) {
                        return valError(web, json, "Invalid %s field '%s' in %s", tag, var->name, web->url);
                    }
                    rDebug("web", "Invalid %s field '%s' in %s", tag, var->name, web->url);
                    continue;
                }
                /*
                    hasWild admits properties the signature does not declare. There is no field
                    signature to check them against, so emit the value unchanged. Passing fid (-1)
                    on to the role, drop and type checks below would fail all of them, and the
                    resulting rejection is what made a wildcard object unusable. The method role
                    still gates the field, as it does for a declared field with no role of its own.
                 */
                if (methodRole && !webCan(web, methodRole)) {
                    omitted = omitField(buf, json, vid);
                    continue;
                }
                if (buf) {
                    jsonPutValueToBuf(buf, var->name, JSON_JSON);
                    rPutCharToBuf(buf, ':');
                    if (jsonPutToBuf(buf, json, vid, JSON_JSON) < 0) {
                        return valError(web, NULL, "Cannot serialize %s", tag);
                    }
                    rPutCharToBuf(buf, ',');
                }
                continue;
            }
            role = jsonGet(signatures, fid, "role", methodRole);
            if (role && !webCan(web, role)) {
                // Silently drop if role does not permit access
                omitted = omitField(buf, json, vid);
                continue;
            }
            drop = jsonGetNode(signatures, fid, "drop");
            if (drop != 0) {
                if (drop->type == JSON_PRIMITIVE && smatch(drop->value, "true")) {
                    omitted = omitField(buf, json, vid);
                    continue;
                } else if (drop->type == JSON_STRING) {
                    if (!webCan(web, drop->value)) {
                        omitted = omitField(buf, json, vid);
                        continue;
                    }
                } else if (drop->type == JSON_OBJECT) {
                    dropRole = jsonGet(signatures, fid, SFMT(dropBuf, "drop.%s", tag), 0);
                    if (dropRole && !webCan(web, dropRole)) {
                        omitted = omitField(buf, json, vid);
                        continue;
                    }
                }
            }
            if (buf) {
                jsonPutValueToBuf(buf, var->name, JSON_JSON);
                rPutCharToBuf(buf, ':');
            }
            ftype = jsonGet(signatures, fid, "type", 0);
            if (smatch(ftype, "object") || smatch(ftype, "array")) {
                //  Validate this occurrence: a lookup by name finds only the first of a repeated name
                if (!webValidateSignature(web, buf, json, vid, fid, depth + 1, tag)) {
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

    /*
        A missing signature is a server-side configuration fault, not a bad request, but it must
        still produce a response. Returning silently left the caller to finalize a request with
        nothing written, so the client saw the connection close with no status at all.
     */
    if (!web->host->signatures || sid < 0) {
        return valError(web, NULL, "Missing %s signature", tag);
    }
    type = getType(web, sid);
    if ((signature = jsonGetNode(web->host->signatures, sid, 0)) == 0) {
        return valError(web, NULL, "Missing %s signature", tag);
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
    if (buf && data) {
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
            if (buf && jsonPutToBuf(buf, cjson, jid, JSON_JSON) < 0) {
                return valError(web, NULL, "Cannot serialize %s", tag);
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
    return (ssize) rGetBufLength(web->buffer);
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
    return (ssize) rGetBufLength(web->buffer);
}

/*
    Check a URL path for valid characters
 */
PUBLIC bool webValidatePath(cchar *path)
{
    size_t pos;

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
        return webError(web, -400, "Empty URL");
    }
    if (!webValidatePath(web->url)) {
        webError(web, -400, "Bad characters in URL");
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
        return webError(web, -400, "Empty URL");
    }
    //  Hash comes after the query
    path = web->url;
    stok(path, "#", &web->hash);
    stok(path, "?", &web->query);

    //  Query is decoded when parsed in webParseQuery and webParseEncoded
    webDecodePath(path);
    webDecode(web->hash);

    /*
        webValidatePath allow-lists the encoded URL, so a control byte can only arrive
        percent-encoded and only shows itself here. One in the path reaches the file system and
        every log line that names the request, where a decoded newline splits the entry and lets
        a request forge a log record. Refuse the whole range; a path never legitimately holds one.
     */
    for (tok = path; *tok; tok++) {
        if ((uchar) * tok < 0x20 || *tok == 0x7f) {
            return webError(web, -400, "Bad characters in URL");
        }
    }

    /*
        Normalize and sanitize the path. This routine will process ".." and "." segments.
        This is safe because callers (webFileHandler) uses simple string concatenation to
        join the result with the document root.
     */
    if ((web->path = webNormalizePath(path)) == 0) {
        return webError(web, -400, "Illegal URL");
    }
    //  Derive the extension from the final decoded filename, not an encoded URL offset.
    web->ext = 0;
    delim = strrchr(web->path, '/');
    if ((tok = strrchr(delim ? delim + 1 : web->path, '.')) != 0 && tok[1]) {
        web->ext = tok;
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
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */

#else
void dummyWeb(){}
#endif /* ME_COM_WEB */
