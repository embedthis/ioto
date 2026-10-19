/*
    auth-users.tst.c - Unit tests for the user, role and ability management API

    These exercise webAddUser, webUpdateUser, webRemoveUser, webLookupUser, webUserCan and the
    password hashing helpers directly against a WebHost, without an HTTP round trip. The role
    hierarchy in web.json5 is: public, user['view','read'], admin['user','edit','delete'],
    owner['admin','billing','manage'], super['owner','super-admin'].

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/
/*
    A username longer than ME_WEB_MAX_AUTH so the length guards can be exercised.
 */
#define OVERLONG_LEN (ME_WEB_MAX_AUTH + 10)

/************************************ Code ************************************/

static WebHost *allocTestHost(void)
{
    WebHost *host;
    Json    *config;

    config = jsonParseFile("web.json5", NULL, 0);
    if (!config) {
        tfail("Cannot parse web.json5");
        return 0;
    }
    host = webAllocHost(config, 0);
    if (!host) {
        tfail("Cannot allocate host");
    }
    return host;
}

/*
    Build the minimal Web fields the auth and cookie routines touch. The connection is never
    started, so there is no socket and no listener.
 */
static Web *allocTestWeb(WebHost *host)
{
    Web *web;

    web = rAllocType(Web);
    web->host = host;
    web->txHeaders = rAllocHash(16, R_DYNAMIC_VALUE);
    web->signature = -1;
    web->status = 200;
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
    Users declared in web.json5 are loaded at host allocation and their abilities expanded
    from the role hierarchy.
 */
static void testConfiguredUsers(WebHost *host)
{
    WebUser *alice, *bob, *missing;

    alice = webLookupUser(host, "alice");
    tnotnull(alice);
    tmatch(alice->role, "admin");

    bob = webLookupUser(host, "bob");
    tnotnull(bob);
    tmatch(bob->role, "user");

    missing = webLookupUser(host, "nosuchuser");
    tnull(missing);

    //  Null tolerance
    tnull(webLookupUser(NULL, "alice"));
    tnull(webLookupUser(host, NULL));
}

/*
    'admin' inherits 'user', which grants 'view' and 'read'. It must not reach 'owner' abilities.
 */
static void testAbilityInheritance(WebHost *host)
{
    WebUser *alice, *bob;

    alice = webLookupUser(host, "alice");
    bob = webLookupUser(host, "bob");
    ttrue(alice && bob);

    //  Direct abilities of admin
    ttrue(webUserCan(alice, "edit"));
    ttrue(webUserCan(alice, "delete"));
    //  Inherited from the 'user' role
    ttrue(webUserCan(alice, "view"));
    ttrue(webUserCan(alice, "read"));
    //  The role names themselves are abilities
    ttrue(webUserCan(alice, "admin"));
    ttrue(webUserCan(alice, "user"));
    //  Not inherited upwards
    tfalse(webUserCan(alice, "billing"));
    tfalse(webUserCan(alice, "manage"));
    tfalse(webUserCan(alice, "super-admin"));

    //  'user' does not reach admin abilities
    ttrue(webUserCan(bob, "view"));
    ttrue(webUserCan(bob, "read"));
    tfalse(webUserCan(bob, "edit"));
    tfalse(webUserCan(bob, "delete"));

    //  Null tolerance and unknown abilities
    tfalse(webUserCan(NULL, "read"));
    tfalse(webUserCan(alice, NULL));
    tfalse(webUserCan(alice, "no-such-ability"));
}

static void testAddUser(WebHost *host)
{
    WebUser *user;
    char    *longName;

    user = webAddUser(host, "carol", "hash", "owner");
    tnotnull(user);
    tmatch(user->role, "owner");
    ttrue(webLookupUser(host, "carol") == user);

    //  owner inherits admin which inherits user
    ttrue(webUserCan(user, "billing"));
    ttrue(webUserCan(user, "manage"));
    ttrue(webUserCan(user, "edit"));
    ttrue(webUserCan(user, "read"));
    tfalse(webUserCan(user, "super-admin"));

    //  A duplicate is refused and leaves the original intact
    tnull(webAddUser(host, "carol", "other", "user"));
    tmatch(webLookupUser(host, "carol")->role, "owner");

    //  A role that is not in the hierarchy cannot be granted
    tnull(webAddUser(host, "mallory", "hash", "no-such-role"));
    tnull(webLookupUser(host, "mallory"));

    //  Missing arguments
    tnull(webAddUser(NULL, "dave", "hash", "user"));
    tnull(webAddUser(host, NULL, "hash", "user"));
    tnull(webAddUser(host, "dave", "hash", NULL));

    //  Length guards on username and password
    longName = rAlloc(OVERLONG_LEN + 1);
    memset(longName, 'a', OVERLONG_LEN);
    longName[OVERLONG_LEN] = '\0';
    tnull(webAddUser(host, longName, "hash", "user"));
    tnull(webAddUser(host, "dave", longName, "user"));
    tnull(webLookupUser(host, "dave"));
    rFree(longName);


    user = webAddUser(host, "erin", NULL, "user");
    tnotnull(user);
    tnull(user->password);
}

static void testUpdateUser(WebHost *host)
{
    WebUser *user;
    char    *longName;

    tnotnull(webAddUser(host, "frank", "oldhash", "user"));

    //  Changing the password leaves the role and abilities alone
    ttrue(webUpdateUser(host, "frank", "newhash", NULL));
    user = webLookupUser(host, "frank");
    tmatch(user->password, "newhash");
    tmatch(user->role, "user");
    ttrue(webUserCan(user, "read"));

    //  Promoting recomputes abilities
    ttrue(webUpdateUser(host, "frank", NULL, "admin"));
    user = webLookupUser(host, "frank");
    tmatch(user->role, "admin");
    tmatch(user->password, "newhash");
    ttrue(webUserCan(user, "edit"));

    //  Demoting drops the abilities the higher role granted
    ttrue(webUpdateUser(host, "frank", NULL, "user"));
    user = webLookupUser(host, "frank");
    ttrue(webUserCan(user, "read"));
    tfalse(webUserCan(user, "edit"));

    //  Passing the stored values back must not read them after they are freed (ASan: bin/sanitize.sh)
    ttrue(webUpdateUser(host, "frank", user->password, user->role));
    user = webLookupUser(host, "frank");
    tmatch(user->password, "newhash");
    tmatch(user->role, "user");

    /*
        An unresolvable role must not be granted. webUpdateUser reports failure and the user is
        left holding no abilities rather than an unverified role name.
     */
    tfalse(webUpdateUser(host, "frank", NULL, "no-such-role"));
    user = webLookupUser(host, "frank");
    tfalse(webUserCan(user, "no-such-role"));

    //  Unknown user and missing arguments
    tfalse(webUpdateUser(host, "nosuchuser", "hash", "user"));
    tfalse(webUpdateUser(NULL, "frank", "hash", "user"));
    tfalse(webUpdateUser(host, NULL, "hash", "user"));

    //  Length guards
    longName = rAlloc(OVERLONG_LEN + 1);
    memset(longName, 'a', OVERLONG_LEN);
    longName[OVERLONG_LEN] = '\0';
    tfalse(webUpdateUser(host, "frank", longName, NULL));
    tfalse(webUpdateUser(host, longName, "hash", NULL));
    rFree(longName);
}

static void testRemoveUser(WebHost *host)
{
    tnotnull(webAddUser(host, "grace", "hash", "user"));
    tnotnull(webLookupUser(host, "grace"));

    ttrue(webRemoveUser(host, "grace"));
    tnull(webLookupUser(host, "grace"));

    //  Removing twice is a no-op, not a crash
    tfalse(webRemoveUser(host, "grace"));
    tfalse(webRemoveUser(host, "nosuchuser"));
    tfalse(webRemoveUser(NULL, "grace"));
    tfalse(webRemoveUser(host, NULL));

    //  The username may be removed by a key owned by the user record itself
    tnotnull(webAddUser(host, "heidi", "hash", "user"));
    ttrue(webRemoveUser(host, webLookupUser(host, "heidi")->username));
    tnull(webLookupUser(host, "heidi"));
}

/*
    web->user is a borrowed pointer into host->users. Removing or re-roling the user must clear
    the borrowed pointer on every in-flight request so a blocked fiber cannot resume onto freed
    memory.
 */
static void testUserRefInvalidation(WebHost *host)
{
    WebUser *user;
    Web     *web;

    tnotnull(webAddUser(host, "ivan", "hash", "admin"));
    user = webLookupUser(host, "ivan");

    web = allocTestWeb(host);
    web->user = user;
    web->authenticated = 1;
    rAddItem(host->webs, web);

    ttrue(webRemoveUser(host, "ivan"));
    tnull(web->user);
    tfalse(web->authenticated);

    //  webCan on a request whose user vanished must deny rather than dereference
    web->authChecked = 1;
    tfalse(webCan(web, "edit"));
    //  A public role is still allowed without a user
    ttrue(webCan(web, "public"));
    ttrue(webCan(web, NULL));
    ttrue(webCan(web, ""));

    rRemoveItem(host->webs, web);
    freeTestWeb(web);
}

static void testGetRole(WebHost *host)
{
    Web *web;

    web = allocTestWeb(host);

    //  Unauthenticated requests have no role
    tnull(webGetRole(web));

    web->user = webLookupUser(host, "alice");
    web->authenticated = 1;
    tmatch(webGetRole(web), "admin");

    //  Authenticated flag without a user must not dereference
    web->user = 0;
    tnull(webGetRole(web));

    freeTestWeb(web);
}

static void testPasswordHashing(WebHost *host)
{
    char *sha, *md5, *again, *other;

    //  The default algorithm is SHA-256 - 64 hex characters
    sha = webHash("password", NULL);
    tnotnull(sha);
    teqz(slen(sha), 64);

    again = webHash("password", "SHA-256");
    tmatch(sha, again);
    rFree(again);

    //  Algorithm names are matched case-insensitively
    again = webHash("password", "sha-256");
    tmatch(sha, again);
    rFree(again);

    //  MD5 is supported for legacy digest interop - 32 hex characters
    md5 = webHash("password", "MD5");
    tnotnull(md5);
    teqz(slen(md5), 32);
    tfalse(smatch(sha, md5));
    rFree(md5);

    //  An unknown algorithm falls back to SHA-256
    again = webHash("password", "no-such-algorithm");
    tmatch(sha, again);
    rFree(again);

    //  Distinct inputs hash differently
    other = webHash("Password", NULL);
    tfalse(smatch(sha, other));
    rFree(other);
    rFree(sha);

    tnull(webHash(NULL, NULL));
}

/*
    Stored passwords are H(username:realm:password), so the same password hashes differently
    for different users.
 */
static void testHashPassword(WebHost *host)
{
    char *alice, *bob, *repeat;

    alice = webHashPassword(host, "alice", "password");
    tnotnull(alice);

    repeat = webHashPassword(host, "alice", "password");
    tmatch(alice, repeat);
    rFree(repeat);

    bob = webHashPassword(host, "bob", "password");
    tnotnull(bob);
    tfalse(smatch(alice, bob));
    rFree(bob);

    //  A different password for the same user differs
    repeat = webHashPassword(host, "alice", "Password");
    tfalse(smatch(alice, repeat));
    rFree(repeat);
    rFree(alice);

    tnull(webHashPassword(NULL, "alice", "password"));
    tnull(webHashPassword(host, NULL, "password"));
    tnull(webHashPassword(host, "alice", NULL));
}

/*
    A stored hash may carry an algorithm prefix that selects how it is verified. All of the forms
    the configuration accepts must verify through this entry point, not only the legacy one.
    web.json5 stores alice as SHA256:, bob as MD5: and ralph as BF1:, each of 'password'.
 */
static void testVerifyUserPassword(WebHost *host)
{
    char *hash;

    //  Legacy unprefixed storage, hashed with the host algorithm
    hash = webHashPassword(host, "judy", "secret");
    tnotnull(hash);
    tnotnull(webAddUser(host, "judy", hash, "user"));
    rFree(hash);

    ttrue(webVerifyUserPassword(host, "judy", "secret"));
    tfalse(webVerifyUserPassword(host, "judy", "wrong"));
    tfalse(webVerifyUserPassword(host, "judy", ""));

    //  SHA256: prefixed
    ttrue(webVerifyUserPassword(host, "alice", "password"));
    tfalse(webVerifyUserPassword(host, "alice", "wrong"));
    tfalse(webVerifyUserPassword(host, "alice", ""));

    //  MD5: prefixed
    ttrue(webVerifyUserPassword(host, "bob", "password"));
    tfalse(webVerifyUserPassword(host, "bob", "wrong"));

    //  BF1: bcrypt, which carries its own salt
    ttrue(webVerifyUserPassword(host, "ralph", "password"));
    tfalse(webVerifyUserPassword(host, "ralph", "wrong"));

    /*
        The hash is bound to the username through the realm, so one user's password does not
        verify against another's stored hash even when the passwords are identical.
     */
    tfalse(webVerifyUserPassword(host, "alice", "secret"));

    //  An unsupported algorithm prefix fails closed rather than falling back
    tnotnull(webAddUser(host, "sha512user", "SHA512:0123456789abcdef", "user"));
    tfalse(webVerifyUserPassword(host, "sha512user", "password"));

    //  Unknown users fail without revealing anything
    tfalse(webVerifyUserPassword(host, "nosuchuser", "secret"));

    tfalse(webVerifyUserPassword(NULL, "judy", "secret"));
    tfalse(webVerifyUserPassword(host, NULL, "secret"));
    tfalse(webVerifyUserPassword(host, "judy", NULL));
}

/*
    An application that keeps its own user store verifies passwords with webCheckPassword. The hashes
    here are what the password tool produces with its default realm, which must be the realm of a host
    that configures neither web.auth.realm nor web.name.
 */
static void testCheckPassword(void)
{
    WebHost *host;
    Json    *config;
    char    *digest, *stored, *legacy;
    cchar   *preimage;

    config = jsonParse("{ web: { documents: './site' } }", 0);
    host = webAllocHost(config, 0);
    tnotnull(host);
    if (!host) {
        jsonFree(config);
        return;
    }
    tmatch(host->realm, WEB_REALM);

    preimage = "carol:" WEB_REALM ":secret";
    digest = cryptGetSha256((cuchar*) preimage, slen(preimage));
    stored = sfmt("SHA256:%s", digest);
    ttrue(webCheckPassword(host, "carol", "secret", stored));
    tfalse(webCheckPassword(host, "carol", "wrong", stored));
    tfalse(webCheckPassword(host, "dave", "secret", stored));
    rFree(stored);
    rFree(digest);

    stored = cryptMakePassword(preimage, 16, 128);
    ttrue(webCheckPassword(host, "carol", "secret", stored));
    tfalse(webCheckPassword(host, "carol", "wrong", stored));
    rFree(stored);

    legacy = webHashPassword(host, "carol", "secret");
    ttrue(webCheckPassword(host, "carol", "secret", legacy));
    rFree(legacy);

    //  A hash made under another realm must not verify
    preimage = "carol:example.com:secret";
    digest = cryptGetSha256((cuchar*) preimage, slen(preimage));
    stored = sfmt("SHA256:%s", digest);
    tfalse(webCheckPassword(host, "carol", "secret", stored));
    rFree(stored);
    rFree(digest);

    tfalse(webCheckPassword(host, "carol", "secret", NULL));
    tfalse(webCheckPassword(NULL, "carol", "secret", "SHA256:00"));
    tfalse(webCheckPassword(host, NULL, "secret", "SHA256:00"));
    tfalse(webCheckPassword(host, "carol", NULL, "SHA256:00"));

    webFreeHost(host);
    jsonFree(config);
}

static void testBase64(void)
{
    char *encoded, *decoded;

    encoded = webEncode64("alice:password");
    tnotnull(encoded);
    tmatch(encoded, "YWxpY2U6cGFzc3dvcmQ=");

    decoded = webDecode64(encoded);
    tmatch(decoded, "alice:password");
    rFree(decoded);
    rFree(encoded);

    //  A single-character round trip exercises base64 padding
    encoded = webEncode64("a");
    tmatch(encoded, "YQ==");
    decoded = webDecode64(encoded);
    tmatch(decoded, "a");
    rFree(decoded);
    rFree(encoded);

    //  An empty input produces no encoded block
    tnull(webEncode64(""));

    tnull(webEncode64(NULL));
    tnull(webDecode64(NULL));
}

static void fiberMain(void *arg)
{
    WebHost *host;

    webInit();
    if ((host = allocTestHost()) != 0) {
        testConfiguredUsers(host);
        testAbilityInheritance(host);
        testAddUser(host);
        testUpdateUser(host);
        testRemoveUser(host);
        testUserRefInvalidation(host);
        testGetRole(host);
        testPasswordHashing(host);
        testHashPassword(host);
        testVerifyUserPassword(host);
        testCheckPassword();
        testBase64();
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
