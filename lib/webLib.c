/*
 * Embedthis Web Library Source
 */

#include "web.h"

#if ME_COM_WEB



/********* Start of file ../../../src/auth.c ************/

/*
    auth.c -- Authorization Management

    This module supports HTTP Basic and Digest authentication with a user/role/ability based authorization scheme.

    In this scheme, Users have passwords and roles. A role grants abilities (permissions) to perform actions.
    Roles can inherit from other roles, creating a hierarchy of permissions.

    Three authentication protocols are supported:
        - HTTP Basic authentication (RFC 7617) - Base64 encoded username:password
        - HTTP Digest authentication (RFC 7616/2617) - Challenge-response with MD5 or SHA-256
        - Session-based authentication - Cached authentication state in sessions

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



/********************************** Forwards **********************************/

static int computeUserAbilities(WebHost *host, WebUser *user);
static int expandRole(WebHost *host, cchar *roleName, RHash *abilities);

#if ME_WEB_HTTP_AUTH
static void sendAuthChallenge(Web *web, WebRoute *route);
#if ME_WEB_AUTH_BASIC
static bool parseBasicAuth(Web *web);
static bool verifyPassword(Web *web, WebUser *user);
static void sendBasicChallenge(Web *web);
#endif
#if ME_WEB_AUTH_DIGEST
static bool parseDigestAuth(Web *web);
static char *computeDigest(Web *web, cchar *password);
static char *createNonce(Web *web, cchar *algorithm);
static bool validateNonce(Web *web);
static void sendDigestChallenge(Web *web, WebRoute *route);
static void cleanupNonces(void *arg);
static void removeNonceEntry(Web *web);
#endif
#endif /* ME_WEB_HTTP_AUTH */

/************************************ Code ************************************/
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
    Add a user to the authentication database
    Password should be pre-hashed: H(username:realm:password)
    Role is a single role name
 */
PUBLIC WebUser *webAddUser(WebHost *host, cchar *username, cchar *password, cchar *role)
{
    WebUser *user;

    if (!host || !username || !password || !role) {
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
    user->password = sclone(password);
    user->role = sclone(role);
    user->abilities = rAllocHash(0, 0);

    //  Compute abilities from role hierarchy
    if (computeUserAbilities(host, user) < 0) {
        webFreeUser(user);
        return NULL;
    }
    rAddName(host->users, username, user, 0);
    return user;
}

/*
    Remove user from authentication database
 */
PUBLIC bool webRemoveUser(WebHost *host, cchar *username)
{
    WebUser *user;

    if (!host || !username) {
        return 0;
    }
    if ((user = webLookupUser(host, username)) != 0) {
        webFreeUser(user);
        return rRemoveName(host->users, username) == 0;
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
        rFree(user->password);
        user->password = sclone(password);
    }
    if (role) {
        rFree(user->role);
        user->role = sclone(role);
        rFree(user->abilities);
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
        rFree(user->abilities);
        rFree(user);
    }
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
    rAddName(abilities, roleName, (void*) 1, 0);

    config = host->config;

    //  Look up role definition - get the roleId first
    if ((roleId = jsonGetId(config, host->roles, roleName)) < 0) {
        rError("web", "Cannot find role %s", roleName);
        return R_ERR_CANT_FIND;
    }
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

/******************* HTTP Authentication (Basic & Digest) ********************/
#if ME_WEB_HTTP_AUTH
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
        if ((user = webLookupUser(web->host, web->username)) == 0) {
            sendAuthChallenge(web, route);
            return 0;
        }
        if (!verifyPassword(web, user)) {
            sendAuthChallenge(web, route);
            return 0;
        }
        //  Success - set authentication state
        web->user = user;
        web->role = user->role;
        web->authenticated = 1;
        return 1;
    }
#endif

#if ME_WEB_AUTH_DIGEST
    if (scaselessmatch(web->authType, "Digest")) {
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
        //  Validate nonce
        if (!validateNonce(web)) {
            sendAuthChallenge(web, route);
            return 0;
        }
        /*
            Enforce URI binding to the actual request-target (normalized server path)
            Note: URI in digest header may be relative (no leading /) or absolute
            If client sent relative URI (no leading /), skip the / in path for comparison
         */
        uri = web->uri;
        if (!uri || !web->path) {
            removeNonceEntry(web);
            sendAuthChallenge(web, route);
            return 0;
        }
        if (*uri != '/' && *web->path == '/') {
            if (!smatch(uri, web->path + 1)) {
                removeNonceEntry(web);
                sendAuthChallenge(web, route);
                return 0;
            }
        } else if (!smatch(uri, web->path)) {
            removeNonceEntry(web);
            sendAuthChallenge(web, route);
            return 0;
        }
        //  Look up user
        if ((user = webLookupUser(web->host, web->username)) == 0) {
            removeNonceEntry(web);
            sendAuthChallenge(web, route);
            return 0;
        }
        //  Compute and verify digest
        web->digest = computeDigest(web, user->password);
        if (!web->digest || !cryptMatch(web->digest, web->digestResponse)) {
            removeNonceEntry(web);
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
        web->role = user->role;
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
#if ME_WEB_AUTH_DIGEST
    if (authType && scaselessmatch(authType, "digest")) {
        sendDigestChallenge(web, route);
        return;
    }
#endif
#if ME_WEB_AUTH_BASIC
    //  Default to Basic authentication
    sendBasicChallenge(web);
#else
    webError(web, 401, "Authentication required but not configured");
#endif
}

/*
    Authenticate the current request.
    This checks if the request has a current session by using the request cookie.
    Returns true if authenticated. Residual: set web->authenticated.
 */
PUBLIC bool webAuthenticate(Web *web)
{
    WebUser *user;
    cchar   *username;

    if (web->authChecked) {
        return web->authenticated;
    }
    web->authChecked = 1;

    if (web->cookie && webGetSession(web, 0) != 0) {
        /*
            SECURITY Acceptable:: Retrieve authentication state from the session storage.
            Faster than re-authenticating.
         */
        if ((username = webGetSessionVar(web, WEB_SESSION_USERNAME, 0)) != 0) {
            rFree(web->username);
            web->username = sclone(username);
            web->role = webGetSessionVar(web, WEB_SESSION_ROLE, 0);
            if (web->role) {
                //  Look up user from session username
                if ((user = webLookupUser(web->host, web->username)) != 0) {
                    //  Verify user still has the cached role
                    if (smatch(user->role, web->role)) {
                        web->user = user;
                        web->authenticated = 1;
                        return 1;
                    } else {
                        rError("web", "User %s role changed from %s to %s", web->username, web->role, user->role);
                    }
                } else {
                    rError("web", "Unknown user in session: %s", web->username);
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
    Login and authorize a user with a given role/ability.
    This creates the login session and defines a session cookie for responses.
    This assumes the caller has already validated the user password.
    The role parameter can be a role name or an ability - checks if user has it.
 */
PUBLIC bool webLogin(Web *web, cchar *username, cchar *role)
{
    WebUser *user;

    assert(web);
    assert(username);
    assert(role);

    rFree(web->username);
    web->username = 0;
    web->role = 0;
    web->user = 0;

    webRemoveSessionVar(web, WEB_SESSION_USERNAME);

    if ((user = webLookupUser(web->host, username)) == 0) {
        rError("web", "Unknown user %s", username);
        return 0;
    }
    //  Verify user has the required ability/role
    if (!webUserCan(user, role)) {
        rError("web", "User %s does not have ability %s", username, role);
        return 0;
    }
    webCreateSession(web);

    webSetSessionVar(web, WEB_SESSION_USERNAME, username);
    web->username = sclone(username);
    web->role = webSetSessionVar(web, WEB_SESSION_ROLE, user->role);  // Store user's actual role
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
    web->role = 0;
    web->user = 0;
    web->authenticated = 0;
    webRemoveSessionVar(web, WEB_SESSION_USERNAME);
    webDestroySession(web);
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
    Hash password for storage
    Format: H(username:realm:password) where H is the algorithm
    Default is SHA-256
 */
PUBLIC char *webHashPassword(WebHost *host, cchar *username, cchar *password)
{
    char  buf[512];
    cchar *realm, *algorithm;

    if (!host || !username || !password) {
        return 0;
    }
    realm = host->realm ? host->realm : host->name;
    algorithm = host->algorithm ? host->algorithm : "SHA-256";
    SFMT(buf, "%s:%s:%s", username, realm, password);
    return webHash(buf, algorithm);
}

/*
    Verify plain-text password against stored hash
    Uses constant-time comparison to prevent timing attacks
 */
PUBLIC bool webVerifyUserPassword(WebHost *host, cchar *username, cchar *password)
{
    WebUser *user;
    char    *hashedPassword;
    bool    match;

    if (!host || !username || !password) {
        return 0;
    }
    if ((user = webLookupUser(host, username)) == 0) {
        return 0;
    }
    hashedPassword = webHashPassword(host, username, password);
    match = cryptMatch(hashedPassword, user->password);
    rFree(hashedPassword);
    return match;
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
    char  buf[512];
    char  *encodedPassword, *storedHash;
    cchar *algorithm, *realm;
    bool  match;

    if (!web->password || !user) {
        return 0;
    }
    /*
        Detect algorithm from password prefix (MD5:, SHA256:, SHA512:, BF1:)
        If no prefix, use host's configured algorithm
     */
    if (sstarts(user->password, "BF1:")) {
        /*
            Bcrypt passwords are verified using cryptCheckPassword()
            which extracts salt and re-encrypts for comparison
         */
        SFMT(buf, "%s:%s:%s", web->username,
             web->host->realm ? web->host->realm : web->host->name,
             web->password);
        return cryptCheckPassword(buf, user->password);

    } else if (sstarts(user->password, "MD5:") ||
               sstarts(user->password, "SHA256:") ||
               sstarts(user->password, "SHA512:")) {
        if (sstarts(user->password, "MD5:")) {
            algorithm = "MD5";
            storedHash = user->password + 4; // Skip "MD5:"
        } else if (sstarts(user->password, "SHA256:")) {
            algorithm = "SHA-256";
            storedHash = user->password + 7; // Skip "SHA256:"
        } else {                             // SHA512 not supported
            return 0;
        }
        //  Hash plain password with detected algorithm
        realm = web->host->realm ? web->host->realm : web->host->name;
        SFMT(buf, "%s:%s:%s", web->username, realm, web->password);
        encodedPassword = webHash(buf, algorithm);
        match = cryptMatch(encodedPassword, storedHash);
        rFree(encodedPassword);
        return match;

    } else {
        //  No prefix - use host's configured algorithm (legacy support)
        if (!web->encoded && web->password) {
            algorithm = web->host->algorithm ? web->host->algorithm : "SHA-256";
            realm = web->host->realm ? web->host->realm : web->host->name;

            //  Encode plain password: H(username:realm:password)
            SFMT(buf, "%s:%s:%s", web->username, realm, web->password);
            encodedPassword = webHash(buf, algorithm);
            rFree(web->password);
            web->password = encodedPassword;
            web->encoded = 1;
        }
        /*
            Use cryptMatch() from crypt module for constant-time comparison
            This prevents timing attacks by always comparing full length
         */
        match = cryptMatch(web->password, user->password);
        return match;
    }
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
    char   macInput[256];
    char   tsRndBuf[128];
    uchar  mac[CRYPT_HMAC_SHA256_SIZE];
    char   *rnd, *result;
    RBuf   *payload;
    cchar  *secret, *realm;
    Ticks  now;
    size_t tsRndLen;

    secret = web->host->secret;
    realm = web->host->realm ? web->host->realm : web->host->name;
    now = rGetTime();
    rnd = cryptID(32);
    SFMT(macInput, "%s:%s:%llx:%s", realm, algorithm ? algorithm : "SHA-256", (long long) now, rnd);
    cryptGetHmacSha256Block((cuchar*) secret, slen(secret), (cuchar*) macInput, slen(macInput), mac);

    // Build payload: timestamp:random:binaryMAC
    SFMT(tsRndBuf, "%llx:%s:", (long long) now, rnd);
    tsRndLen = slen(tsRndBuf);
    payload = rAllocBuf(tsRndLen + CRYPT_HMAC_SHA256_SIZE);
    rPutBlockToBuf(payload, tsRndBuf, tsRndLen);
    rPutBlockToBuf(payload, (char*) mac, CRYPT_HMAC_SHA256_SIZE);

    result = cryptEncode64Block((cuchar*) rGetBufStart(payload), rGetBufLength(payload));
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
    char          *whenStr, *rnd, *tok, macInput[256];
    int           age, currentNc, rc;
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
        age = (int) ((now - when) / TPS);
        if (age >= 0 && age <= web->host->digestTimeout) {
            // Compute expected HMAC using same inputs
            realm = web->host->realm ? web->host->realm : web->host->name;
            secret = web->host->secret;
            algorithm = (web->route && web->route->algorithm) ? web->route->algorithm :
                        (web->host->algorithm ? web->host->algorithm : "SHA-256");
            SFMT(macInput, "%s:%s:%s:%s", realm, algorithm, whenStr, rnd);
            cryptGetHmacSha256Block((cuchar*) secret, slen(secret), (cuchar*) macInput, slen(macInput), expectedMac);
            match = cryptMatchHmacSha256(expectedMac, receivedMac);
            if (match) {
                // Replay protection: validate nonce count (nc) is incrementing
                // Skip tracking if trackNonces is disabled (for testing/benchmarks)
                if (web->nc && web->host->trackNonces) {
                    currentNc = (int) stoix(web->nc, NULL, 16);
                    entry = (WebNonceEntry*) rLookupName(web->host->nonces, web->nonce);
                    if (entry) {
                        // Nonce exists - validate nc is incrementing
                        if (currentNc > entry->lastNc) {
                            // Update last nc value
                            entry->lastNc = currentNc;
                            rc = 1;
                        }
                    } else {
                        if (rGetHashLength(web->host->nonces) > web->host->maxDigest) {
                            cleanupNonces(web->host);
                        }
                        if (rGetHashLength(web->host->nonces) <= web->host->maxDigest) {
                            // First use of this nonce - create tracking entry
                            entry = rAllocType(WebNonceEntry);
                            entry->created = when;
                            entry->lastNc = currentNc;
                            rAddName(web->host->nonces, web->nonce, entry, 0);
                            rc = 1;
                        } else {
                            static bool warned = false;
                            if (!warned) {
                                rError("web", "Digest authentication nonce limit reached: %d", web->host->maxDigest);
                                warned = true;
                            }
                        }
                    }
                } else {
                    // No replay protection if qop not used or tracking disabled
                    rc = 1;
                }
            }
        }
    }
    rFree(decoded);
    return rc;
}

/*
    Remove a nonce entry from the tracking hash
    Called when authentication fails to prevent memory leak from failed attempts
 */
static void removeNonceEntry(Web *web)
{
    WebNonceEntry *entry;

    if (web->nonce && web->host && web->host->nonces) {
        entry = (WebNonceEntry*) rLookupName(web->host->nonces, web->nonce);
        if (entry) {
            rRemoveName(web->host->nonces, web->nonce);
        }
    }
}

/*
    Clean up expired nonces from tracking hash
    @param arg WebHost pointer
 */
static void cleanupNonces(void *arg)
{
    WebHost       *host;
    WebNonceEntry *entry;
    RName         *np;
    RList         *toDelete;
    Ticks         now, cutoff, period;
    char          *nonceName;
    int           next;

    host = (WebHost*) arg;

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
                rRemoveName(host->nonces, nonceName);
            }
        }
        rFreeList(toDelete);
    }
    period = min(30, host->digestTimeout / 2);
    host->nonceCleanupEvent = rAllocEvent(NULL, (REventProc) cleanupNonces, host, period * TPS, 0);
}

/*
    Initialize digest authentication (start nonce cleanup timer)
 */
PUBLIC void webInitDigestAuth(WebHost *host)
{
    Ticks period;

    period = min(30, host->digestTimeout / 2);
    host->nonceCleanupEvent = rAllocEvent(NULL, (REventProc) cleanupNonces, host, period * TPS, 0);
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

    nonce = createNonce(web, algorithm);

    webSetStatus(web, 401);
    webAddHeader(web, "WWW-Authenticate",
                 "Digest realm=\"%s\", qop=\"auth\", nonce=\"%s\", opaque=\"%s\", algorithm=\"%s\"",
                 realm, nonce, opaque, algorithm);
    rFree(nonce);
    webFinalize(web);
}

/*
    Parse Digest authentication header
    Format: Digest username="...", realm="...", nonce="...", uri="...", response="...", ...
 */
static bool parseDigestAuth(Web *web)
{
    char *key, *value, *tok, *details, *opaque;

    if (!web->authDetails) {
        return 0;
    }
    details = sclone(web->authDetails);
    key = details;

    while (*key) {
        //  Skip whitespace
        while (*key && isspace((uchar) * key)) key++;
        //  Find key
        tok = key;
        for (tok = key; *tok && *tok != '=' && *tok != ','; tok++) {
        }
        if (*tok != '=') {
            break;
        }
        *tok++ = '\0';
        //  Parse value (may be quoted)
        if (*tok == '"') {
            value = ++tok;
            while (*tok && *tok != '"') tok++;
            if (*tok == '"') *tok++ = '\0';
        } else {
            value = tok;
            while (*tok && *tok != ',') tok++;
        }
        if (*tok == ',') *tok++ = '\0';

        //  Store parsed values
        if (scaselessmatch(key, "username")) {
            rFree(web->username);
            web->username = sclone(value);
        } else if (scaselessmatch(key, "realm")) {
            rFree(web->realm);
            web->realm = sclone(value);
        } else if (scaselessmatch(key, "nonce")) {
            rFree(web->nonce);
            web->nonce = sclone(value);
        } else if (scaselessmatch(key, "uri")) {
            rFree(web->uri);
            web->uri = sclone(value);
        } else if (scaselessmatch(key, "qop")) {
            rFree(web->qop);
            web->qop = sclone(value);
        } else if (scaselessmatch(key, "nc")) {
            rFree(web->nc);
            web->nc = sclone(value);
        } else if (scaselessmatch(key, "algorithm")) {
            rFree(web->algorithm);
            web->algorithm = sclone(value);
        } else if (scaselessmatch(key, "cnonce")) {
            rFree(web->cnonce);
            web->cnonce = sclone(value);
        } else if (scaselessmatch(key, "response")) {
            rFree(web->digestResponse);
            web->digestResponse = sclone(value);
        } else if (scaselessmatch(key, "opaque")) {
            rFree(web->opaque);
            web->opaque = sclone(value);
        }
        key = tok;
    }
    rFree(details);

    //  Validate required fields
    if (!web->username || !web->digestResponse || !web->realm || !web->nonce || !web->uri) {
        return 0;
    }
    //  Validate field lengths (prevent buffer overflows)
    if (slen(web->username) > 64 || slen(web->nonce) > 256 || slen(web->uri) > 2048 || slen(web->realm) > 128) {
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
    return 1;
}

/*
    Compute digest response per RFC 7616
    response = H(HA1:nonce:nc:cnonce:qop:HA2)
    where HA1 = H(username:realm:password), HA2 = H(method:uri)
 */
static char *computeDigest(Web *web, cchar *password)
{
    char  a2Buf[512], digestBuf[1024];
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
    SFMT(a2Buf, "%s:%s", web->method, web->uri);
    ha2 = webHash(a2Buf, algorithm);

    //  Final digest
    if (web->qop && scaselessmatch(web->qop, "auth")) {
        SFMT(digestBuf, "%s:%s:%s:%s:%s:%s", ha1, web->nonce, web->nc, web->cnonce, web->qop, ha2);
    } else {
        SFMT(digestBuf, "%s:%s:%s", ha1, web->nonce, ha2);
    }
    result = webHash(digestBuf, algorithm);

    rFree(ha1);
    rFree(ha2);
    return result;
}
#endif /* ME_WEB_AUTH_DIGEST */

#endif /* ME_WEB_HTTP_AUTH */

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

static int deleteFile(Web *web, char *path, size_t pathSize);
static int fixRanges(Web *web, int64 fileSize);
static int getFile(Web *web, char *path, size_t pathSize);
static cchar *getEncoding(Web *web);
static int sendFile(Web *web, int fd, FileInfo *info, cchar *encoding);
static int pickRanges(Web *web, FileInfo *info, cchar *etag);
static int putFile(Web *web, char *path, size_t pathSize);
static void redirectToDir(Web *web);
static bool pickFile(Web *web, char path[ME_MAX_FNAME], FileInfo *info, cchar **pEncoding);
static int sendFileContent(Web *web, int fd, FileInfo *info);
static void writeRangeHeader(Web *web, WebRange *range, int64 fileSize);

/************************************* Code ***********************************/

PUBLIC int webFileHandler(Web *web)
{
    char path[ME_MAX_FNAME];
    int  rc;

    /*
        SECURITY Acceptable:: the path is already validated and normalized in
        webValidateRequest / webNormalizePath
     */
    sjoinbuf(path, sizeof(path), webGetDocs(web->host), web->path);

    if (web->get || web->head || web->post) {
        rc = getFile(web, path, sizeof(path));
    } else if (web->put) {
        rc = putFile(web, path, sizeof(path));    // PUT always uses original path
    } else if (web->del) {
        rc = deleteFile(web, path, sizeof(path)); // DELETE uses original path
    } else {
        rc = webError(web, 405, "Unsupported method");
    }
    return rc;
}

static int getFile(Web *web, char *path, size_t pathSize)
{
    FileInfo info;
    cchar    *encoding;
    int      fd, rc;

    if (!pickFile(web, path, &info, &encoding)) {
        webHook(web, WEB_HOOK_NOT_FOUND);
        if (!web->finalized) {
            return webError(web, 404, "Cannot locate document");
        }
        return 0;
    }
    if (web->finalized) {
        return 0;
    }
    if ((fd = open(path, O_RDONLY | O_BINARY, 0)) < 0) {
        webError(web, 404, "Cannot open document");
        return R_ERR_CANT_OPEN;
    }
    rc = sendFile(web, fd, &info, encoding);
    close(fd);
    return rc;
}

static int sendFile(Web *web, int fd, FileInfo *info, cchar *encoding)
{
    char etag[24];
    int  rc;

    //  Generate unquoted ETag for faster comparison
    sitosbuf(etag, sizeof(etag), (int64) ((uint64) info->st_ino ^ (uint64) info->st_size ^ (uint64) info->st_mtime), 10);

    /*
        Check conditional request headers (If-None-Match, If-Modified-Since)
        Return 304 Not Modified if content hasn't changed
        Per RFC 7232, this check happens before processing ranges
     */
    rc = 0;
    if (webContentNotModified(web, etag, info->st_mtime)) {
        web->txLen = 0;
        web->status = 304;
        webAddHeaderStaticString(web, "Accept-Ranges", "bytes");
        webAddHeaderDynamicString(web, "Last-Modified", webHttpDate(info->st_mtime));
        webAddHeader(web, "ETag", "\"%s\"", etag);

    } else if (pickRanges(web, info, etag) < 0) {
        webError(web, 416, "Requested range not satisfiable");

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
            webAddHeaderStaticString(web, "Vary", "Origin, Accept-Encoding");
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
}

static int putFile(Web *web, char *path, size_t pathSize)
{
    FileInfo info;
    char     etag[24], *ptr;
    size_t   bufsize;
    ssize    nbytes;
    int      fd, total;

    /*
        Check preconditions for state-changing requests per RFC 7232
        If-Match and If-Unmodified-Since ensure the client has the current version
     */
    if ((web->ifMatchPresent || web->ifUnmodified) && stat(path, &info) == 0) {
        sitosbuf(etag, sizeof(etag), (int64) ((uint64) info.st_ino ^ (uint64) info.st_size ^ (uint64) info.st_mtime), 10);

        //  Check If-Match precondition (must match to proceed)
        if (web->ifMatchPresent && !webMatchEtag(web, etag)) {
            return webError(web, 412, "Precondition not satisfied");
        }
        //  Check If-Unmodified-Since precondition (must be unmodified to proceed)
        if (web->ifUnmodified && !webMatchModified(web, info.st_mtime)) {
            return webError(web, 412, "Precondition not satisfied");
        }
        web->exists = 1;
    } else {
        web->exists = stat(path, &info) == 0;
    }
    assert(rGetBufLength(web->body) == 0);

    if ((fd = open(path, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, 0600)) < 0) {
        return webError(web, 404, "Cannot open document");
    }
    total = 0;
    //  Zero-copy: read directly from rx buffer
    bufsize = min(ME_BUFSIZE * 16, (size_t) web->rxRemaining);
    while ((nbytes = webReadDirect(web, &ptr, bufsize)) > 0) {
        if (write(fd, ptr, (uint) nbytes) != nbytes) {
            close(fd);
            return webError(web, 500, "Cannot put document");
        }
        total += (int) nbytes;
        if (total > web->host->maxUpload) {
            close(fd);
            unlink(path);
            return webError(web, 414, "Uploaded put file exceeds maximum %lld", web->host->maxUpload);
        }
    }
    close(fd);
    if (nbytes < 0) {
        unlink(path);
        return (int) webWriteResponseString(web, 500, "PUT request failed with premature client disconnect");
    }
    if (web->rxRemaining > 0) {
        unlink(path);
        return (int) webWriteResponseString(web, 500, "PUT request received insufficient body data");
    }
    return (int) webWriteResponseString(web, web->exists ? 204 : 201, "Document successfully updated");
}

static int deleteFile(Web *web, char *path, size_t pathSize)
{
    FileInfo info;
    char     etag[24];

    /*
        Check preconditions for state-changing requests per RFC 7232
        If-Match and If-Unmodified-Since ensure the client has the current version
     */
    if (web->ifMatchPresent || web->ifUnmodified) {
        if (stat(path, &info) == 0) {
            sitosbuf(etag, sizeof(etag), (int64) ((uint64) info.st_ino ^ (uint64) info.st_size ^ (uint64) info.st_mtime), 10);

            //  Check If-Match precondition (must match to proceed)
            if (web->ifMatchPresent && !webMatchEtag(web, etag)) {
                return webError(web, 412, "Precondition not satisfied");
            }
            //  Check If-Unmodified-Since precondition (must be unmodified to proceed)
            if (web->ifUnmodified && !webMatchModified(web, info.st_mtime)) {
                return webError(web, 412, "Precondition not satisfied");
            }
        } else {
            return webError(web, 404, "Cannot locate document");
        }
    }
    if (unlink(path) != 0) {
        return webError(web, 404, "Cannot delete document");
    }
    return (int) webWriteResponseString(web, 204, "Document successfully deleted");
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
    //  Use zero-copy sendfile for non-TLS HTTP connections
    if (!rIsSocketSecure(web->sock)) {
        written = rSendFile(web->sock, fd, offset, (size_t) len);
        if (written < 0 || written < len) {
            return webNetError(web, "Cannot send file");
        }
        return written;
    }
#endif
    //  Seek to offset if non-zero
    if (offset > 0 && lseek(fd, (off_t) offset, SEEK_SET) != offset) {
        return webError(web, 500, "Cannot seek in file");
    }
    bufSize = len < ME_BUFSIZE ? ME_BUFSIZE : ME_BUFSIZE * 4;
    buf = rAlloc(bufSize);
    for (written = 0; written < len; ) {
        toRead = min(len - written, (ssize) bufSize);
        if ((nbytes = read(fd, buf, (uint) toRead)) < 0) {
            rFree(buf);
            return webError(web, 404, "Cannot read document");
        }
        if (nbytes == 0) {
            rFree(buf);
            return webError(web, 404, "Premature end of input");
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
    return 0;
}

/*
    Process and validate range requests
    Returns 0 on success, error code on failure
 */
static int pickRanges(Web *web, FileInfo *info, cchar *etag)
{
    WebRange *range;
    bool     rangeValid;

    if (!web->ranges) {
        //  Default case: no ranges requested - serve full file
        web->status = 200;
        web->txLen = info->st_size;
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
            web->ranges = NULL;
            web->status = 200;
            web->txLen = info->st_size;
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
        web->rangeBoundary = cryptID(16);
        rFree(web->rmime);
        web->mime = web->rmime = sfmt("multipart/byteranges; boundary=%s", web->rangeBoundary);
        web->txLen = -1;  // Unknown length for chunked encoding
    } else {
        //  Single range - set Content-Range header
        range = web->ranges;
        webAddHeader(web, "Content-Range", "bytes %lld-%lld/%lld", web->ranges->start, range->end - 1, (int64) info->st_size);
        web->txLen = range->len;
    }
    return 0;
}

/*
    Write Content-Range header for multipart boundary
 */
static void writeRangeHeader(Web *web, WebRange *range, int64 fileSize)
{
    webWriteFmt(web, "\r\n--%s\r\n", web->rangeBoundary);
    webWriteFmt(web, "Content-Type: %s\r\n", web->mime ? web->mime : "application/octet-stream");
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
            if (webSendFile(web, fd, range->start, range->len) < 0) {
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
static bool pickFile(Web *web, char path[ME_MAX_FNAME], FileInfo *info, cchar **encoding)
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
        sncat(path, ME_MAX_FNAME, web->host->index);
        web->exists = stat(path, info) == 0;
        web->ext = strrchr(web->host->index, '.');
    }
    len = slen(path);

    if (web->route->compressed && (*encoding = getEncoding(web)) != 0) {
        if (smatch(*encoding, "br")) {
            sncat(path, ME_MAX_FNAME, ".br");
            found = stat(path, info) == 0;
        }
        if (!found && smatch(*encoding, "gzip")) {
            sncat(path, ME_MAX_FNAME, ".gz");
            found = stat(path, info) == 0;
        }
    }
    if (found) {
        web->exists = 1;
    } else {
        // Remove failed .br, .gz extensions
        path[len] = '\0';
        *encoding = NULL;
        web->exists = stat(path, info) == 0;
        if (web->exists && (info->st_mode & S_IFDIR) != 0) {
            // External redirect to the directory
            redirectToDir(web);
            return 0;
        }
    }
    return web->exists;
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
static void loadAuth(WebHost *host);
static void parseCacheControl(WebRoute *route, Json *json, int id);
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
    host->connSequence = 0;

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
    host->maxDigest = svaluei(jsonGet(host->config, 0, "web.limits.digest", "1000"));
    host->maxBuffer = svaluei(jsonGet(host->config, 0, "web.limits.buffer", "64K"));
    host->maxBody = svaluei(jsonGet(host->config, 0, "web.limits.body", "100K"));
    host->maxConnections = svaluei(jsonGet(host->config, 0, "web.limits.connections", "100"));
    host->maxHeader = svaluei(jsonGet(host->config, 0, "web.limits.header", "10K"));
    host->maxSessions = svaluei(jsonGet(host->config, 0, "web.limits.sessions", "20"));
    host->maxUpload = svaluei(jsonGet(host->config, 0, "web.limits.upload", "20MB"));
    host->maxUploads = svaluei(jsonGet(host->config, 0, "web.limits.uploads", "0"));
    host->maxRequests = svaluei(jsonGet(host->config, 0, "web.limits.requests", "1000"));
#endif

    host->docs = rGetFilePath(jsonGet(host->config, 0, "web.documents", "@site"));
    host->name = jsonGet(host->config, 0, "web.name", 0);
    host->uploadDir = jsonGet(host->config, 0, "web.upload.dir", uploadDir());
    host->sessionCookie = jsonGet(host->config, 0, "web.sessions.cookie", WEB_SESSION_COOKIE);
    host->sameSite = jsonGet(host->config, 0, "web.sessions.sameSite", "Lax");
    host->httpOnly = jsonGetBool(host->config, 0, "web.sessions.httpOnly", 1);
    host->roles = jsonGetId(host->config, 0, "web.auth.roles");
    host->headers = jsonGetId(host->config, 0, "web.headers");

    host->webSocketsMaxMessage = svaluei(jsonGet(host->config, 0, "web.limits.maxMessage", "100K"));
    host->webSocketsMaxFrame = svaluei(jsonGet(host->config, 0, "web.limits.maxFrame", "100K"));
    host->webSocketsPingPeriod = svaluei(jsonGet(host->config, 0, "web.webSockets.ping", "never"));
    host->webSocketsProtocol = jsonGet(host->config, 0, "web.webSockets.protocol", "chat");
    host->webSocketsEnable = jsonGetBool(host->config, 0, "web.webSockets.enable", 1);
    host->webSocketsValidateUTF = jsonGetBool(host->config, 0, "web.webSockets.validateUTF", 0);

    initMethods(host);
    initRoutes(host);
    initRedirects(host);
    loadMimeTypes(host);
    loadAuth(host);
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

#if ME_WEB_HTTP_AUTH
    // Free HTTP authentication configuration (realm, authType, algorithm come from config - not cloned)
    rFree(host->secret);
    rFree(host->opaque);
#if ME_WEB_AUTH_DIGEST
    // Free nonce tracking hash table and stop cleanup timer
    if (host->nonceCleanupEvent) {
        rStopEvent(host->nonceCleanupEvent);
    }
    if (host->nonces) {
        rFreeHash(host->nonces);
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
    int64 value;

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
    route->cacheMaxAge = (int) svalue(jsonGet(json, cacheId, "maxAge", NULL));
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
    Load authentication configuration from web.json5
    Loads users, roles, and HTTP authentication settings
 */
static void loadAuth(WebHost *host)
{
    Json     *json;
    JsonNode *users, *user;
    cchar    *algorithm, *username, *password, *role, *secret;


    json = host->config;

    // Initialize users hash table (always available for session-based auth)
    host->users = rAllocHash(0, 0);

#if ME_WEB_HTTP_AUTH
    // Load HTTP authentication settings (use config strings directly - they persist with host->config)
    host->realm = jsonGet(json, 0, "web.auth.realm", host->name ? host->name : "web");
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
    host->requireTlsForBasic = jsonGetBool(json, 0, "web.auth.requireTlsForBasic", 0);
    host->opaque = cryptID(32);
    // Generate random secret if not provided
    secret = jsonGet(json, 0, "web.auth.secret", 0);
    if (secret) {
        host->secret = sclone(secret);
    } else {
        // Generate 64-character random alphanumeric ID for HMAC secret
        host->secret = cryptID(64);
    }

#if ME_WEB_AUTH_DIGEST
    // Initialize nonce tracking for replay protection
    // Use R_DYNAMIC_VALUE since we're storing allocated WebNonceEntry pointers, not strings
    host->trackNonces = jsonGetBool(json, 0, "web.auth.track", 1);
    host->nonces = rAllocHash(0, R_TEMPORAL_NAME | R_DYNAMIC_VALUE);
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
                if (webAddUser(host, username, password, role) < 0) {
                    rError("web", "Cannot add user %s", username);
                }
            }
        }
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
    Valid characters for HTTP header field names per RFC 7230.
    Allows: A-Z, a-z, 0-9, and special characters: ! # $ % & ' * + - . ^ _ ` | ~
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

/************************************ Forwards *********************************/

static bool authenticateRequest(Web *web);
static void freeWebFields(Web *web, bool keepAlive);
static int handleRequest(Web *web);
static bool matchFrom(Web *web, cchar *from);
static int parseHeaders(Web *web, size_t headerSize);
static int parseMethod(Web *web, cchar *method);
static bool parseEtags(Web *web, cchar *value);
static bool parseRangeHeader(Web *web, char *header);
static int processBody(Web *web);
static void processOptions(Web *web);
static void processQuery(Web *web);
static int redirectRequest(Web *web);
static void resetWeb(Web *web);
static bool routeRequest(Web *web);
static bool shouldCacheControl(Web *web, WebRoute *route);
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
    rFree(web->rmime);
    web->rmime = NULL;
}

/*
    Free the web instance object fields. This is called when the request is complete.
    The web instance object is preserved for the next request if keepAlive is true.
 */
static void freeWebFields(Web *web, bool keepAlive)
{
    RSocket   *sock;
    WebListen *listen;
    Ticks     connectionStarted;
    RBuf      *rx, *rxHeaders, *body, *buffer;
    RList     *etags;
    int64     conn, count;
    int       close;

    /*
        If keepAlive is true, we need to save some fields for the next request.
     */
    if (keepAlive) {
        close = web->close;
        conn = web->conn;
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
    webFreeUpload(web);
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
    if (mask & R_TIMEOUT) {
        rTrace("web", "Keep-alive inactivity timeout on connection %lld", web->conn);
        web->close = 1;
    } else {

#if FIBER_BLOCKS
        /*
            Future code to catch exceptions during request processing
            This is like a try/catch block and aborts
            This needs to be configurable and opt-in
         */
        if (rStartFiberBlock() == 0) {
#endif
        web->fiber = rGetFiber();

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

            if (rGetBufLength(web->rx) == 0) {
                //  No buffered data, setup wait for next request
                webSetupKeepAliveWait(web);
                return;
            }
            //  Continue loop to process pipelined requests
        }
#if FIBER_BLOCKS
    } else {
        /*
            Note: this may not perform full cleanup of resources allocated by the request
            This is a best effort to allow the server to continue serving other requests
         */
        rLog("raw", "web", "Exception in request processing");
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

    if (rGetTimeouts()) {
        if (web->count > 0) {
            web->deadline = min(web->started + web->host->inactivityTimeout,
                                web->started + web->host->requestTimeout);
        } else {
            web->deadline = rGetTimeouts() ? web->started + web->host->parseTimeout : 0;
        }
    } else {
        web->deadline = 0;
    }

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
        // I/O error or pattern not found before limit
        return R_ERR_CANT_READ;
    }
    web->count++;
    web->headerSize = size;

    if (parseHeaders(web, (size_t) size) < 0) {
        return R_ERR_BAD_REQUEST;
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
    cchar *handler;

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
    if (web->route->xsrf) {
        if (web->get) {
            /*
                This will generate a new XSRF token if there is not one already in the session state.
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
    char *path, *cp;
    size_t len, urlLen;
    bool rc;

    assert(web);
    host = web->host;

    len = slen(web->route->match);
    urlLen = slen(web->url);
    if (urlLen < len) {
        return 0;
    }
    rc = 1;
    if (host->signatures) {
        path = sclone(&web->url[len]);
        for (cp = path; *cp; cp++) {
            if (*cp == '/') *cp = '.';
        }
        web->signature = jsonGetId(host->signatures, 0, path);
        if (web->route->validate) {
            rc = webValidateRequest(web, path);
        }
        rFree(path);
    }
    return rc;
}

/*
    Handle an action request. This is called when the request is a valid action request.
    It will run the action function and return the result.
 */
static int webActionHandler(Web *web)
{
    WebAction *action;
    int next;

    for (ITERATE_ITEMS(web->host->actions, action, next)) {
        /*
            Check if the request path matches the action match pattern.
         */
        if (sstarts(web->path, action->match)) {
            /*
                For public actions (role == NULL or "public"), do not deny access.
                Attempt authorization only if a specific non-public role is required.
             */
            if (action->role && !smatch(action->role, "public")) {
                if (!webCan(web, action->role)) {
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
    char *path;
    bool match;
    int next;

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

            } else if (route->role && !smatch(route->role, "public") && !web->options) {
                if (!authenticateRequest(web)) {
                    return 0;
                }
                if (!webCan(web, route->role)) {
                    webError(web, 403, "Access Denied. User has insufficient privilege.");
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

    if (!web->error) {
        webWriteResponseString(web, 404, "No matching route");
    }
    return 0;
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
static bool parseRangeHeader(Web *web, char *header)
{
    WebRange *range, *last;
    int64 end, len, start;
    char *tok, *ep, *value;

    //  Must start with "bytes="
    if (!sstarts(header, "bytes=")) {
        return 0;
    }
    header += 6;  // Skip "bytes="
    last = NULL;

    //  Parse comma-separated ranges
    for (tok = stok(header, ",", (char**) &value); tok; tok = stok(NULL, ",", (char**) &value)) {
        tok = strim(tok, " \t", R_TRIM_BOTH);
        start = end = len = 0;

        //  Parse start-end
        if (*tok == '-') {
            /*
                Suffix range: -500 means last 500 bytes
                Validate that the rest is numeric
             */
            if (!tok[1] || !isdigit((uchar) tok[1])) {
                return 0;
            }
            start = -1;
            end = stoi(tok + 1);
            if (end <= 0) {
                return 0;
            }
        } else if ((ep = strchr(tok, '-')) != NULL) {
            //  Validate start is numeric
            if (!isdigit((uchar) * tok)) {
                return 0;
            }
            start = stoi(tok);
            if (start < 0) {
                return 0;
            }
            if (ep[1] == '\0') {
                //  Open-ended: 500- means from 500 to end
                end = -1;
            } else {
                /*
                    Normal range: 0-499. Validate end is numeric.
                 */
                if (!isdigit((uchar) ep[1])) {
                    return 0;
                }
                end = stoi(ep + 1) + 1;  // +1 for exclusive end
                if (end < 0) {
                    return 0;
                }
            }
        } else {
            return 0;
        }
        //  Validate range
        if (start >= 0 && end >= 0) {
            if (start >= end) {
                return 0;
            }
            len = end - start;
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
    char *end, *method, *tok;

    buf = web->rx;
    if (headerSize <= 10 || headerSize > (size_t) rGetBufLength(buf)) {
        return webNetError(web, "Bad request header");
    }
    end = buf->start + headerSize;
    end[-2] = '\0';

    rPutBlockToBuf(web->rxHeaders, buf->start, headerSize - 2);
    rAdjustBufStart(buf, end - buf->start);

    if (web->host->flags & WEB_SHOW_REQ_HEADERS) {
        rLog("raw", "web", "Request <<<<\n\n%s\n", web->rxHeaders->start);
    }
    method = supper(stok(web->rxHeaders->start, " \t", &tok));
    if (parseMethod(web, method) < 0) {
        return R_ERR_BAD_REQUEST;
    }
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

static int parseMethod(Web *web, cchar *method)
{
    if (!method) {
        return webNetError(web, "Bad request method");
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

/*
    Parse a headers block. Used here and by file upload.
 */
PUBLIC bool webParseHeadersBlock(Web *web, char *headers, size_t headersSize, bool upload)
{
    cchar *end;
    char c, *cp, *endKey, *key, *prior, *t, *value;
    uchar uc;
    bool hasCL = 0, hasTE = 0;

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
                uc = (uchar) * t;
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
#if ME_WEB_HTTP_AUTH
            if (c == 'a' && scaselessmatch(key, "authorization")) {
                //  Parse Authorization header: "Basic xxx" or "Digest xxx"
                char *authType = value;
                char *sp = strchr(value, ' ');
                if (sp) {
                    *sp++ = '\0';
                    web->authType = sclone(authType);
                    web->authDetails = sclone(sp);
                }
            } else
#endif
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
                    if (web->rxLen < 0) {
                        webError(web, -400, "Bad Content-Length");
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

            } else if (c == 'i') {
                if (scaselessmatch(key, "if-match")) {
                    if (!parseEtags(web, value)) {
                        webError(web, 400, "Invalid If-Match header");
                        return 0;
                    }
                    web->ifMatchPresent = 1;

                } else if (scaselessmatch(key, "if-modified-since")) {
                    web->since = rParseHttpDate(value);
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
                    //  Can be either an ETag or a date - strip quotes for faster comparison
                    if (*value == '"') {
                        web->ifMatch = sclone(strim((char*) value, "\"", R_TRIM_BOTH));
                    } else if (*value == 'W' && value[1] == '/' && value[2] == '"') {
                        //  Weak ETag: strip W/ prefix and quotes
                        web->ifMatch = sclone(strim((char*) value + 2, "\"", R_TRIM_BOTH));
                    } else {
                        //  Date format - parse it into web->since (will be used for conditional range)
                        web->since = rParseHttpDate(value);
                    }
                    web->ifRange = 1;

                } else if (scaselessmatch(key, "if-unmodified-since")) {
                    web->unmodifiedSince = rParseHttpDate(value);
                    if (web->unmodifiedSince > 0) {
                        web->ifUnmodified = 1;
                    }
                }

            } else if (c == 'l' && scaselessmatch(key, "last-event-id")) {
                web->lastEventId = stoi(value);

            } else if (c == 'o' && scaselessmatch(key, "origin")) {
                web->origin = value;

            } else if (c == 'r' && scaselessmatch(key, "range")) {
                value = sclone(value);
                if (!parseRangeHeader(web, value)) {
                    rFree(value);
                    webError(web, 400, "Invalid Range header");
                    return 0;
                }
                rFree(value);

            } else if (c == 't' && scaselessmatch(key, "transfer-encoding")) {
                if (scaselessmatch(value, "chunked")) {
                    hasTE = 1;
                    web->chunked = WEB_CHUNK_START;
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
    Not used for streamed, websockets, or PUT requests
 */
PUBLIC int webReadBody(Web *web)
{
    WebRoute *route;
    RBuf *buf;
    ssize nbytes;

    route = web->route;
    if ((route && route->stream) || web->webSocket || web->put || (web->rxRemaining <= 0 && !web->chunked)) {
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
        if (rGetBufLength(buf) > (size_t) web->host->maxBody) {
            webError(web, -413, "Request is too big");
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
    bool hasNoCache, hasNoStore;

    route = web->route;
    if (!route) {
        return;
    }
    if (!shouldCacheControl(web, route)) {
        return;
    }
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

    /*
        Set Expires and Pragma headers for HTTP/1.0 compatibility
        HTTP/1.1+ clients understand Cache-Control and don't need these headers
     */
    if (web->http10) {
        hasNoCache = route->cacheDirectives && scontains(route->cacheDirectives, "no-cache");
        hasNoStore = route->cacheDirectives && scontains(route->cacheDirectives, "no-store");

        if (route->cacheMaxAge > 0 && !hasNoCache && !hasNoStore) {
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

static char *findPatternFrom(RBuf *buf, cchar *pattern, size_t patLen, size_t fromOffset);
static bool isprintable(cchar *s, size_t len);
static ssize consumeChunkStart(Web *web, size_t desiredSize);
static int consumeChunkData(Web *web, ssize nbytes);
static ssize readSocketBuffer(Web *web, size_t desiredSize);
static ssize readSocketBlock(Web *web, size_t desiredSize);
static int writeChunkDivider(Web *web, size_t bufsize);

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
    RBuf   *bp;
    ssize  nbytes;
    size_t outstanding;

    bp = web->rx;

    if (!web->chunked) {
        outstanding = max(rGetBufLength(bp), (size_t) web->rxRemaining);
        bufsize = min(bufsize, outstanding);
    }
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
    //  Copy to user buffer
    memcpy(buf, bp->start, (size_t) nbytes);
    if (consumeChunkData(web, nbytes) < 0) {
        return R_ERR_CANT_READ;
    }
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
        return R_ERR_CANT_READ;
    }
    rAdjustBufEnd(bp, nbytes);
    web->rxRead += nbytes;
    return nbytes;
}

/*
    Parse chunk header and transition from WEB_CHUNK_START to WEB_CHUNK_DATA.
    Returns desiredSize (capped to chunkRemaining) on success, 0 on EOF, negative on error.
 */
static ssize consumeChunkStart(Web *web, size_t desiredSize)
{
    ssize chunkSize;
    char  cbuf[32];

    if (web->chunked == WEB_CHUNK_EOF) {
        return 0;
    }
    if (web->chunked == WEB_CHUNK_START) {
        if (webReadUntil(web, "\r\n", cbuf, sizeof(cbuf)) < 0) {
            return webError(web, -400, "Bad chunk data");
        }
        cbuf[sizeof(cbuf) - 1] = '\0';
        chunkSize = (ssize) stoix(cbuf, NULL, 16);
        if (chunkSize < 0) {
            return webError(web, -400, "Bad chunk specification");
        }
        if (chunkSize == 0) {
            //  Zero chunk -- end of body
            if (webReadUntil(web, "\r\n", cbuf, sizeof(cbuf)) < 0) {
                return webError(web, -400, "Bad chunk data");
            }
            web->chunkRemaining = 0;
            web->rxRemaining = 0;
            web->chunked = WEB_CHUNK_EOF;
            return 0;
        }
        web->chunkRemaining = chunkSize;
        web->chunked = WEB_CHUNK_DATA;
    }
    //  Cap desiredSize to chunkRemaining
    return (ssize) min(desiredSize, (size_t) web->chunkRemaining);
}

/*
    Consume data from the rx buffer and update chunk state.
    Handles rAdjustBufStart, chunkRemaining, rxRemaining, and trailing CRLF.
    Returns 0 on success, negative on error.
 */
static int consumeChunkData(Web *web, ssize nbytes)
{
    char cbuf[32];

    if (nbytes <= 0) {
        return 0;
    }
    rAdjustBufStart(web->rx, nbytes);

    if (web->chunked == WEB_CHUNK_DATA) {
        web->chunkRemaining -= nbytes;
        if (web->chunkRemaining <= 0) {
            web->chunked = WEB_CHUNK_START;
            web->chunkRemaining = WEB_UNLIMITED;
            if (webReadUntil(web, "\r\n", cbuf, sizeof(cbuf)) < 0) {
                return webNetError(web, "Bad chunk data");
            }
        }
    } else if (web->chunked == WEB_CHUNK_EOF) {
        web->rxRemaining = 0;
    } else {
        web->rxRemaining -= nbytes;
    }
    webUpdateDeadline(web);
    return 0;
}

/*
    Internal: Low level read and buffer.
    Fill the rx buffer from socket without chunk handling.
    Returns bytes available in buffer or negative on error.
 */
static ssize readSocketBuffer(Web *web, size_t desiredSize)
{
    RBuf   *bp;
    ssize  nbytes;
    size_t available, bufsize, toRead;

    bp = web->rx;

    //  If data already in buffer, return available bytes
    available = rGetBufLength(bp);
    if (available > 0) {
        return (ssize) min(available, desiredSize);
    }
    //  If no more body data expected (and buffer is empty), return EOF
    if (web->rxRemaining == 0) {
        return 0;
    }
    /*
        Size the buffer as large as possible to minimize the number of socket reads.
        Limit to the remaining body data, the desired size or 64K
     */
    bufsize = max(desiredSize, ME_BUFSIZE * 4);
    if (web->rxRemaining > 0 && (size_t) web->rxRemaining < bufsize) {
        bufsize = (size_t) web->rxRemaining;
    }
    if (bufsize <= ME_BUFSIZE) {
        bufsize = ME_BUFSIZE;
    }
    rCompactBuf(bp);
    rGrowBufSize(bp, bufsize);
    toRead = rGetBufSpace(bp);
    if (web->rxRemaining > 0) {
        toRead = min(toRead, (size_t) web->rxRemaining);
    }
    if ((nbytes = readSocket(web, toRead, web->deadline)) < 0) {
        return webNetError(web, "Cannot read from socket");
    }
    return (ssize) min(rGetBufLength(bp), desiredSize);
}

/*
    Internal: Fill the rx buffer with request body data without copying.
    Returns the number of bytes available in web->rx buffer, or negative on error.
    Handles chunk header parsing for chunked transfer encoding.
    Does NOT consume data - caller must call consumeChunkData() after processing.
 */
static ssize readSocketBlock(Web *web, size_t desiredSize)
{
    ssize size;

    if (web->chunked) {
        if ((size = consumeChunkStart(web, desiredSize)) <= 0) {
            return size;  // 0 for EOF, negative for error
        }
        desiredSize = (size_t) size;
    }
    return readSocketBuffer(web, desiredSize);
}

/*
    Read request body data directly from the rx buffer (zero-copy).
    Fills buffer and sets *dataPtr to the data location. Consumes internally before returning.
    Returns bytes available, or 0 on EOF, or negative on error.

    Usage pattern:
        char *ptr;
        ssize nbytes;
        while ((nbytes = webReadDirect(web, &ptr, ME_BUFSIZE * 4)) > 0) {
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

    //  Consume data internally (adjusts start pointer, handles chunk state)
    if (consumeChunkData(web, nbytes) < 0) {
        return R_ERR_CANT_READ;
    }
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
        //  Copy data into the supplied buffer
        len = (ssize) min((size_t) nbytes, limit);
        memcpy(buf, bp->start, (size_t) len);
        rAdjustBufStart(bp, len);
    }
    return nbytes;
}

/*
    Read until the specified pattern is seen or until the size limit.
    This may read headers and body data for this request.
    If reading headers, we may read all the body data and (WARNING) any pipelined request following.
    If chunked, we may also read a subsequent pipelined request. May call webNetError.
    Return the total number of buffered bytes up to the requested pattern.
    Return zero if pattern not found before limit or negative on errors.

    Uses incremental scanning: tracks how much has been scanned and only scans new data
    plus a safety overlap of pattern length to handle patterns split across reads.
 */
PUBLIC ssize webBufferUntil(Web *web, cchar *pattern, size_t limit)
{
    RBuf   *bp;
    char   *end;
    size_t patLen, toRead, scanned, scanFrom;
    ssize  nbytes;

    assert(web);
    assert(limit >= 0);
    assert(pattern);

    bp = web->rx;
    patLen = slen(pattern);
    scanned = 0;

    while (1) {
        //  Scan from (scanned - patLen) to handle patterns split across reads
        scanFrom = (scanned > patLen) ? scanned - patLen : 0;

        if ((end = findPatternFrom(bp, pattern, patLen, scanFrom)) != 0) {
            break;
        }
        if (rGetBufLength(bp) >= limit) {
            //  Pattern not found before limit
            return 0;
        }
        //  Mark current buffer as fully scanned before reading more
        scanned = rGetBufLength(bp);

        rCompactBuf(bp);
        rReserveBufSpace(bp, limit - rGetBufLength(bp));
        toRead = rGetBufSpace(bp);
        if (limit) {
            toRead = min(toRead, limit - rGetBufLength(bp));
        }
        if (toRead <= 0) {
            //  Pattern not found before the limit
            return 0;
        }
        if ((nbytes = readSocket(web, toRead, web->deadline)) < 0) {
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
static char *findPatternFrom(RBuf *buf, cchar *pattern, size_t patLen, size_t fromOffset)
{
    char   *cp, *endp, *searchStart;
    size_t bufLen;

    assert(buf);

    bufLen = rGetBufLength(buf);
    if (bufLen < patLen || fromOffset < 0) {
        return 0;
    }
    searchStart = buf->start + fromOffset;
    endp = buf->start + (bufLen - patLen) + 1;

    if (searchStart >= endp) {
        return 0;
    }
    for (cp = searchStart; cp < endp; cp++) {
        if ((cp = (char*) memchr(cp, *pattern, (size_t) (endp - cp))) == 0) {
            return 0;
        }
        if ((size_t) (buf->end - cp) < patLen) {
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

    if (web->upgrade) {
        connection = "Upgrade";
    } else if (web->close) {
        connection = "close";
    } else {
        connection = "keep-alive";
        remaining = host->requestTimeout - (rGetTicks() - web->connectionStarted);
        webAddHeader(web, "Keep-Alive", "timeout=%lld, max=%d", remaining / TPS, host->maxRequests - web->count);
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
        web->mime = rLookupName(host->mimeTypes, web->ext);
    }
    if (web->mime) {
        webAddHeaderStaticString(web, "Content-Type", web->mime);
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
    protocol = web->protocol ? web->protocol : "HTTP/1.1";

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
    rAddDuplicateName(web->txHeaders, key, value, R_DYNAMIC_VALUE);
}

PUBLIC void webAddHeaderStaticString(Web *web, cchar *key, cchar *value)
{
    rAddDuplicateName(web->txHeaders, key, (void*) value, R_STATIC_VALUE);
}

/*
    Add an Access-Control-Allow-Origin response header necessary for CORS requests.
 */
PUBLIC void webAddAccessControlHeader(Web *web)
{
    char  *hostname;
    cchar *schema, *contentEncoding;

    /*
        Check if Content-Encoding is set (pre-compressed content)
        If so, include both Origin and Accept-Encoding in Vary header
     */
    if (!rLookupName(web->txHeaders, "Vary")) {
        contentEncoding = rLookupName(web->txHeaders, "Content-Encoding");
        if (contentEncoding) {
            webAddHeaderStaticString(web, "Vary", "Origin, Accept-Encoding");
        } else {
            webAddHeaderStaticString(web, "Vary", "Origin");
        }
    }
    if (web->origin) {
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
    ssize written;

    if (web->finalized) {
        return 0;
    }
    if (buf == NULL) {
        bufsize = 0;
    } else if (bufsize == 0 || bufsize >= MAXINT) {
        bufsize = slen(buf);
    }
    if (web->buffer && !web->writingHeaders) {
        if (buf) {
            rPutBlockToBuf(web->buffer, buf, bufsize);
            return (ssize) bufsize;
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
            if (isprintable(buf, (size_t) written)) {
                if (web->moreBody) {
                    write(rGetLogFile(), (char*) buf, (uint) written);
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

    webWriteResponseString(web, status, NULL);
    rFree(freeHost);
    rFree(buf);
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
    return createSession(web);
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
        if (id) {
            session = rLookupName(web->host->sessions, id);
            rFree(id);
        }
        if (!session && create) {
            session = createSession(web);
        }
        web->session = session;
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
        webError(web, 429, "Failed to create session");
        return 0;
    }
    if ((session = webAllocSession(web, host->sessionTimeout)) == 0) {
        webError(web, 429, "Failed to create session");
        return 0;
    }
    webSetCookie(web, web->host->sessionCookie, session->id, "/", 0, 0);
    web->session = session;
    return session;
}

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
        webError(web, 429, "Failed to create session");
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
    RList      *expired;
    int        count, oldCount, next;

    when = rGetTicks();
    oldCount = rGetHashLength(host->sessions);

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
    rFreeList(expired);

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
    Set a response cookie
 */
PUBLIC int webSetCookie(Web *web, cchar *name, cchar *value, cchar *path, Ticks lifespan, int flags)
{
    WebHost *host;
    cchar   *httpOnly, *secure, *sameSite;
    Ticks   maxAge;

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
    return 0;
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



#if ME_COM_WEBSOCK
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
    if ((key = webGetHeader(web, "sec-websocket-key")) == 0) {
        webError(web, 400, "Bad Sec-WebSocket-Key");
        return R_ERR_BAD_ARGS;
    }
    webSetStatus(web, 101);
    webAddHeaderStaticString(web, "Upgrade", "WebSocket");

    sjoinbuf(keybuf, sizeof(keybuf), key, WS_MAGIC);
    webAddHeaderDynamicString(web, "Sec-WebSocket-Accept", cryptGetSha1Base64(keybuf, 0));

    protocol = webSocketGetProtocol(web->webSocket);
    if (protocol && *protocol) {
        webAddHeaderStaticString(web, "Sec-WebSocket-Protocol", protocol);
    }
    webAddHeader(web, "X-Request-Timeout", "%lld", web->host->requestTimeout / TPS);
    webAddHeader(web, "X-Inactivity-Timeout", "%lld", web->host->inactivityTimeout / TPS);
    webFinalize(web);
    return 0;
}

#endif /* ME_COM_WEBSOCK */

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



#if ME_DEBUG || ME_BENCHMARK
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
    jsonSetFmt(json, 0, "connection", "%ld", web->conn);
    jsonSetFmt(json, 0, "count", "%ld", web->count);

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
    webWriteResponseString(web, 200, "error\n");
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
    webWriteResponse(web, 200, "%d\n", total);
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
    rFree(web->username);
    web->username = sclone("user");

    if (web->vars) {
        webWriteValidatedJson(web, web->vars, NULL);
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

    name = webGetQueryVar(web, "name", 0);
    value = webGetQueryVar(web, "value", 0);
    path = webGetQueryVar(web, "path", "/path");

    if (!name || !value || !path) {
        webError(web, 400, "Missing name or value");
        return;
    }
    if (webSetCookie(web, name, value, path, 0, 0) < 0) {
        webError(web, 404, "Invalid cookie");
        return;
    }
    webWriteResponseString(web, 200, "success");
}


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

#if ME_COM_WEBSOCK
/*
    Echo back user provided message.
    On connected, buf will be null
 */
static void onEvent(WebSocket *ws, int event, cchar *buf, size_t len, Web *web)
{
    if (event == WS_EVENT_MESSAGE) {
        // rTrace("test", "Echoing: %ld bytes", len);
        // Echo back with the same message type (preserves binary/text)
        // Use ws->type for accumulated message type (handles multi-frame messages)
        webSocketSendBlock(ws, ws->type, buf, len);
    }
}

static void webSocketAction(Web *web)
{
    if (!web->upgrade) {
        webError(web, 400, "Connection not upgraded to WebSocket");
        return;
    }
    webSocketRun(web->webSocket, (WebSocketProc) onEvent, web, web->rx, web->host->inactivityTimeout);
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
    webAddAction(host, SFMT(url, "%s/bench", prefix), successAction, NULL);
    webAddAction(host, SFMT(url, "%s/put", prefix), putAction, NULL);
    webAddAction(host, SFMT(url, "%s/show", prefix), showAction, NULL);
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



/*********************************** Forwards *********************************/
#if ME_WEB_UPLOAD

static WebUpload *allocUpload(Web *web, cchar *name, cchar *filename);
static void freeUpload(WebUpload *up);
static size_t getUploadDataLength(Web *web);
static int processUploadData(Web *web);
static int processUploadHeaders(Web *web);

/************************************* Code ***********************************/

PUBLIC int webInitUpload(Web *web)
{
    char *boundary;

    if ((boundary = scontains(web->contentType, "boundary=")) != 0) {
        web->boundary = sjoin("--", &boundary[9], NULL);
        web->boundaryLen = slen(web->boundary);
    }
    if (web->boundaryLen == 0 || *web->boundary == '\0') {
        webError(web, 400, "Bad boundary");
        return R_ERR_BAD_ARGS;
    }
    web->uploads = rAllocHash(0, 0);
    web->numUploads = 0;
    if (!web->vars) {
        web->vars = jsonAlloc();
    }
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
    if ((filename = rGetTempFile(web->host->uploadDir, "tmp")) == 0) {
        webError(web, 500, "Cannot create upload temp file. Check upload directory configuration");
        rFree(path);
        return 0;
    }
    rTrace("web", "File upload of: %s stored as %s", path, filename);

    if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0600)) < 0) {
        webError(web, 500, "Cannot open upload temp file");
        rFree(path);
        rFree(filename);
        return 0;
    }
    if ((upload = rAllocType(WebUpload)) == 0) {
        // Memory errors handled globally
        webError(web, 500, "Cannot allocate upload");
        close(fd);
        return 0;
    }
    upload->name = sclone(name);
    upload->clientFilename = path;
    upload->filename = filename;
    upload->fd = fd;
    upload->size = 0;

    return upload;
}

static void freeUpload(WebUpload *upload)
{
    if (upload->filename) {
        unlink(upload->filename);
        rFree(upload->filename);
    }
    if (upload->fd >= 0) {
        close(upload->fd);
        upload->fd = -1;
    }
    rFree(upload->clientFilename);
    rFree(upload->name);
    rFree(upload->contentType);
    rFree(upload);
}

PUBLIC int webProcessUpload(Web *web)
{
    RBuf  *buf;
    char  suffix[8];
    ssize nbytes;

    buf = web->rx;
    while (1) {
        if (web->host->maxUploads > 0 && ++web->numUploads > web->host->maxUploads) {
            return webError(web, 413, "Too many files uploaded");
        }
        if ((nbytes = webBufferUntil(web, web->boundary, ME_BUFSIZE * 2)) <= 0) {
            return webError(web, -400, "Bad upload request boundary");
        }
        rAdjustBufStart(buf, nbytes);

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
    web->rxRemaining = 0;
    return 0;
}

static int processUploadHeaders(Web *web)
{
    WebUpload *upload;
    char      *content, *field, *key, *next, *rest, *value, *type;
    ssize     nbytes;

    if ((nbytes = webBufferUntil(web, "\r\n\r\n", ME_BUFSIZE * 2)) < 2) {
        webError(web, -400, "Bad upload headers");
        return R_ERR_BAD_REQUEST;
    }
    content = web->rx->start;
    content[nbytes - 2] = '\0';
    rAdjustBufStart(web->rx, nbytes);

    if (web->host->flags & WEB_SHOW_REQ_HEADERS) {
        rLog("raw", "web", "Upload Header %d <<<<\n\n%s\n", (int) web->rx->buflen, content);
    }

    /*
        The mime headers may contain Content-Disposition and Content-Type headers
     */
    for (key = content; key && stok(key, "\r\n", &next); ) {
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
                    if ((upload = allocUpload(web, web->uploadName, value)) == 0) {
                        return R_ERR_CANT_COMPLETE;
                    }
                    rAddName(web->uploads, upload->name, upload, 0);
                    web->upload = upload;
                }
                field = rest;
            }
        } else if (scaselessmatch(key, "Content-Type")) {
            type = strim(rest, " ", R_TRIM_BOTH);
            if (web->upload) {
                rFree(web->upload->contentType);
                web->upload->contentType = sclone(type);
            }
        } else if (!web->uploadName) {
            webError(web, -400, "Bad upload headers. Missing Content-Disposition name");
            return R_ERR_BAD_REQUEST;
        }
        key = next;
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
    size_t    len;
    ssize     nbytes, written;

    upload = web->upload;
    buf = web->rx;
    do {
        if ((nbytes = webBufferUntil(web, web->boundary, ME_BUFSIZE * 16)) < 0) {
            return webError(web, -400, "Bad upload request boundary");
        }
        if (upload && upload->fd >= 0) {
            /*
                If webBufferUntil returned 0 (short), then a complete boundary was not seen. In this case,
                write the data and continue but preserve a possible partial boundary with \r\n delimiter.
             */
            if (nbytes) {
                /*
                    Extract the data before the \r\n delimiter and boundary
                 */
                len = (size_t) max(0, (size_t) nbytes - web->boundaryLen - 2);
            } else {
                /*
                    Not a complete boundary, so preserve a possible partial boundary.
                 */
                len = getUploadDataLength(web);
            }
            if (len > 0) {
                if ((upload->size + len) > (size_t) web->host->maxUpload) {
                    if (upload->fd >= 0) {
                        close(upload->fd);
                        upload->fd = -1;
                    }
                    return webError(web, 414, "Uploaded file exceeds maximum %lld", web->host->maxUpload);
                }
                if ((written = write(upload->fd, buf->start, (uint) len)) < 0) {
                    if (upload->fd >= 0) {
                        close(upload->fd);
                        upload->fd = -1;
                    }
                    return webError(web, 500, "Cannot write uploaded file");
                }
                rAdjustBufStart(buf, (ssize) len);
                upload->size += (size_t) written;
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
            if (nbytes < 3) {
                return webError(web, -400, "Bad upload form data");
            }
            buf->start[nbytes - 2] = '\0';
            data = webDecode(buf->start);
            webSetVar(web, web->uploadName, data);
            if (web->host->flags & WEB_SHOW_REQ_HEADERS) {
                rLog("raw", "web", "Upload Form Field <<<<\n\n%s = %s\n", web->uploadName, data);
            }
            rAdjustBufStart(buf, nbytes);
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
    RBuf  *buf;
    cchar *cp, *from;

    buf = web->rx;
    from = max(buf->start, buf->end - web->boundaryLen - 2);

    /*
        Check if the first character of the boundary "-" is found at the end of the buffer
        Start search at the end of the buffer, less the boundary length and \r\n delimiter
     */
    if ((cp = memchr(from, web->boundary[0], (size_t) (buf->end - from))) != NULL) {
        return (size_t) max(0, cp - buf->start - 2);
    }
    return (size_t) max(0, rGetBufLength(buf) - 2);
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

/*
    HTTP date is always 29 characters long 
    Format as RFC 7231 IMF-fixdate: "Mon, 10 Nov 2025 21:28:28 GMT"
    Caller must free
 */
PUBLIC char *webHttpDate(time_t when)
{
    struct tm tm;
    char      buf[32];

#if ME_WIN_LIKE
    if (gmtime_s(&tm, &when) != 0) {
        return 0;
    }
#else
    if (gmtime_r(&when, &tm) == 0) {
        return 0;
    }
#endif
    buf[0] = '\0';
    if (strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", &tm) == 0) {
        return 0;
    }
    return sclone(buf);
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
    char   *ip,  *op;
    size_t len;
    int    num, i, c;

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
        char *sp = segments[i];
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
    Get a request query variable from the request URL.
 */
PUBLIC cchar *webGetQueryVar(Web *web, cchar *name, cchar *defaultValue)
{
    return jsonGet(web->qvars, 0, name, defaultValue);
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
        if (!web->vars) {
            web->vars = jsonAlloc();
        }
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
        for (ITERATE_JSON(signatures, fields, field, fid)) {
            required = jsonGet(signatures, fid, "required", 0);
            if (required) {
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
        return webError(web, -400, "Illegal URL");
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
