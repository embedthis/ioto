/*
    auth-negative.tst.c - HTTP Basic and Digest authentication refusals

    The auth tests cover the happy paths. These drive every failure branch: for Basic, over TLS
    with a hand-built Authorization header; for Digest, over a raw socket with the response
    computed here from the parsed challenge so that each field can be corrupted on its own. A
    refusal must be a 401 challenge (or 403 for a wrong role) that carries none of the protected
    document, and a correct exchange for the same account must succeed, or the negatives prove
    nothing.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

//  HA1 = SHA-256("user:Test Realm:password"), the stored form in web.json5. wendy is the user-role account.
#define ALICE_HA1 "11c66702489123a02c7dd0860e47fc4989bb0805ba5835fbbb36f316ec83eb83"
#define WENDY_HA1 "266613436ac90192ef2125c9fdb87d200799d7edecb08ba945edbc6654029f1f"

/*
    Digest fields the test controls. NULL fields take the value from the challenge or a default.
 */
typedef struct Digest {
    cchar *username;
    cchar *ha1;
    cchar *uri;          //  Defaults to the request path
    cchar *realm;        //  Defaults to "Test Realm", the realm configured in web.json5
    cchar *nonce;        //  Defaults to the challenge nonce
    cchar *opaque;       //  Defaults to the challenge opaque
    cchar *algorithm;    //  Defaults to the challenge algorithm
    cchar *nc;           //  Defaults to 00000001
    cchar *cnonce;       //  Semantic value; used to compute the response
    cchar *cnonceWire;   //  Bytes sent on the wire for cnonce; defaults to cnonce unescaped
    cchar *response;     //  Overrides the computed response
    cchar *qop;          //  Defaults to auth. Any other value is hashed in the RFC 2069 form.
    bool  noQop;         //  Omit qop, nc and cnonce (RFC 2069 form)
    bool  noCnonce;      //  Omit cnonce
    bool  noResponse;    //  Omit the response field
    cchar *extra;        //  Appended to the Authorization header after the response field
} Digest;

/************************************ Code ************************************/

/*
    Copy a quoted parameter value out of a WWW-Authenticate header
 */
static bool param(cchar *header, cchar *name, char *value, size_t size)
{
    cchar *cp, *end;
    char  key[64];

    SFMT(key, "%s=\"", name);
    if ((cp = scontains(header, key)) == 0) {
        return 0;
    }
    cp += slen(key);
    if ((end = schr(cp, '"')) == 0) {
        return 0;
    }
    sncopy(value, size, cp, (size_t) (end - cp));
    return 1;
}

/*
    Request the path without credentials and parse the Digest challenge
 */
static bool challenge(cchar *endpoint, cchar *path, char *nonce, char *opaque, char *algorithm, size_t size)
{
    cchar *segments[1];
    char  req[512], buf[8192], *header;
    int   status;

    SFMT(req, "GET %s HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", path);
    segments[0] = req;
    status = rawRequest(endpoint, segments, 1, 0, 1, buf, sizeof(buf));
    teqi(status, 401, "%s must challenge an anonymous request", path);
    if ((header = sncaselesscontains(buf, "WWW-Authenticate: Digest", slen(buf))) == 0) {
        tfail("No Digest challenge for %s", path);
        return 0;
    }
    return param(header, "nonce", nonce, size) && param(header, "opaque", opaque, size) &&
           param(header, "algorithm", algorithm, size);
}

/*
    Send a GET with a Digest Authorization built from the fields. Returns the status and whether
    the response carried the protected document.
 */
static int digestGet(cchar *endpoint, cchar *requestPath, Digest *d, cchar *nonce, cchar *opaque, cchar *algorithm,
                     bool *sawBody)
{
    cchar *segments[1];
    char  *ha2, *response, *secret, a2[512], rbuf[1024], qopPart[128], responsePart[128], header[2048];
    char  req[3072], buf[8192], path[256], cnoncePart[128];
    cchar *uri, *realm, *nc, *cnonce, *cnonceWire, *body, *qop;
    int   status;

    uri = d->uri ? d->uri : requestPath;
    realm = d->realm ? d->realm : "Test Realm";
    nonce = d->nonce ? d->nonce : nonce;
    opaque = d->opaque ? d->opaque : opaque;
    algorithm = d->algorithm ? d->algorithm : algorithm;
    nc = d->nc ? d->nc : "00000001";
    // cnonce is the decoded value; cnonceWire includes quoting escapes.
    cnonce = d->cnonce ? d->cnonce : "0a4f113b";
    cnonceWire = d->cnonceWire ? d->cnonceWire : cnonce;
    qop = d->qop ? d->qop : "auth";

    SFMT(a2, "GET:%s", uri);
    ha2 = cryptGetSha256((cuchar*) a2, slen(a2));
    if (d->noQop || !smatch(qop, "auth")) {
        SFMT(rbuf, "%s:%s:%s", d->ha1, nonce, ha2);
    } else {
        SFMT(rbuf, "%s:%s:%s:%s:auth:%s", d->ha1, nonce, nc, cnonce, ha2);
    }
    response = cryptGetSha256((cuchar*) rbuf, slen(rbuf));
    if (d->response) {
        rFree(response);
        response = sclone(d->response);
    }
    qopPart[0] = responsePart[0] = '\0';
    if (!d->noQop) {
        cnoncePart[0] = '\0';
        if (!d->noCnonce) {
            SFMT(cnoncePart, ", cnonce=\"%s\"", cnonceWire);
        }
        SFMT(qopPart, ", qop=%s, nc=%s%s", qop, nc, cnoncePart);
    }
    if (!d->noResponse) {
        SFMT(responsePart, ", response=\"%s\"", response);
    }
    SFMT(header, "Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", "
         "algorithm=\"%s\", opaque=\"%s\"%s%s%s", d->username, realm, nonce, uri, algorithm, opaque, qopPart,
         responsePart, d->extra ? d->extra : "");
    SFMT(req, "GET %s HTTP/1.1\r\nHost: x\r\nConnection: close\r\nAuthorization: %s\r\n\r\n", requestPath, header);
    segments[0] = req;
    status = rawRequest(endpoint, segments, 1, 0, 1, buf, sizeof(buf));
    *sawBody = 0;
    if ((body = scontains(buf, "\r\n\r\n")) != 0) {
        //  The protected document itself is the signal: compare against what is on disk
        if ((secret = rReadFile(SFMT(path, "site%s", requestPath), NULL)) == 0) {
            tfail("Cannot read site%s", requestPath);
        } else {
            *sawBody = scontains(body, strim(secret, "\r\n", R_TRIM_END)) != 0;
            rFree(secret);
        }
    }
    rFree(ha2);
    rFree(response);
    return status;
}

/*
    Run one digest exchange against the shared server and check the status and body
 */
static void digestCase(cchar *path, Digest *d, int expected, cchar *why)
{
    char nonce[512], opaque[128], algorithm[32];
    bool sawBody;
    int  status;

    if (!challenge(HTTP, path, nonce, opaque, algorithm, sizeof(nonce))) {
        return;
    }
    status = digestGet(HTTP, path, d, nonce, opaque, algorithm, &sawBody);
    teqi(status, expected, "%s", why);
    if (expected != 200) {
        ttrue(!sawBody, "%s: the protected document must not be served", why);
    } else {
        ttrue(sawBody, "%s: the document must be served", why);
    }
}

static void testDigestRefusals(void)
{
    char   longName[80], longNonce[300];
    Digest good = { .username = "alice", .ha1 = ALICE_HA1 };
    Digest unknown = { .username = "nobody", .ha1 = ALICE_HA1 };
    Digest wrongPassword = { .username = "alice", .ha1 = WENDY_HA1 };
    Digest wrongUri = { .username = "alice", .ha1 = ALICE_HA1, .uri = "/digest/other.html" };
    Digest wrongOpaque = { .username = "alice", .ha1 = ALICE_HA1, .opaque = "forged" };
    Digest badAlgorithm = { .username = "alice", .ha1 = ALICE_HA1, .algorithm = "MD4" };
    Digest md5OnSha = { .username = "alice", .ha1 = ALICE_HA1, .algorithm = "MD5" };
    Digest noResponse = { .username = "alice", .ha1 = ALICE_HA1, .noResponse = 1 };
    Digest badResponse = { .username = "alice", .ha1 = ALICE_HA1, .response = "not-a-hash" };
    Digest badNc = { .username = "alice", .ha1 = ALICE_HA1, .nc = "1" };
    Digest longUser = { .username = longName, .ha1 = ALICE_HA1 };
    Digest forgedNonce = { .username = "alice", .ha1 = ALICE_HA1, .nonce = longNonce };
    Digest noQop = { .username = "alice", .ha1 = ALICE_HA1, .noQop = 1 };
    Digest authInt = { .username = "alice", .ha1 = ALICE_HA1, .qop = "auth-int" };
    Digest noCnonce = { .username = "alice", .ha1 = ALICE_HA1, .noCnonce = 1 };
    Digest userRole = { .username = "wendy", .ha1 = WENDY_HA1 };
    // A valid digest alone does not validate the supplied realm.
    Digest wrongRealm = { .username = "alice", .ha1 = ALICE_HA1, .realm = "TOTALLY-WRONG" };
    // Escape the quote on the wire while hashing the decoded cnonce.
    //  A token without a value must fail the parse, not end it with the parameters read so far
    Digest trailingJunk = { .username = "alice", .ha1 = ALICE_HA1, .extra = ", junk, username=\"mallory\"" };
    Digest emptyElements = { .username = "alice", .ha1 = ALICE_HA1, .extra = ", ," };
    Digest quotedPairCnonce = {
        .username = "alice", .ha1 = ALICE_HA1, .cnonce = "ab\"cd", .cnonceWire = "ab\\\"cd"
    };

    memset(longName, 'a', sizeof(longName) - 1);
    longName[sizeof(longName) - 1] = '\0';
    memset(longNonce, 'n', sizeof(longNonce) - 1);
    longNonce[sizeof(longNonce) - 1] = '\0';

    digestCase("/digest/secret.html", &good, 200, "A correct digest is accepted");
    digestCase("/digest/secret.html", &trailingJunk, 401, "A token without a value after the response is challenged");
    digestCase("/digest/secret.html", &emptyElements, 200, "Empty list elements are skipped");
    digestCase("/digest/secret.html", &noQop, 401, "The qop-less form has no replay protection and is challenged");
    digestCase("/digest/secret.html", &authInt, 401, "qop=auth-int is not offered and is challenged");
    digestCase("/digest/secret.html", &noCnonce, 401, "qop=auth without a cnonce is challenged");
    digestCase("/digest/secret.html", &unknown, 401, "An unknown user is challenged");
    digestCase("/digest/secret.html", &wrongPassword, 401, "A wrong password is challenged");
    digestCase("/digest/secret.html", &wrongUri, 401, "A uri that is not the request target is challenged");
    digestCase("/digest/secret.html", &wrongOpaque, 401, "An opaque the server did not issue is challenged");
    digestCase("/digest/secret.html", &badAlgorithm, 401, "An algorithm outside the allow-list is challenged");
    digestCase("/digest/secret.html", &md5OnSha, 401, "MD5 on a SHA-256 route is challenged");
    digestCase("/digest/secret.html", &noResponse, 401, "A missing response field is challenged");
    digestCase("/digest/secret.html", &badResponse, 401, "A garbage response is challenged");
    digestCase("/digest/secret.html", &badNc, 401, "A nonce count that is not eight hex digits is challenged");
    digestCase("/digest/secret.html", &longUser, 401, "An over-long username is challenged");
    digestCase("/digest/secret.html", &forgedNonce, 401, "A nonce the server did not issue is challenged");
    digestCase("/admin/secret.html", &userRole, 403, "A user-role account on an admin route is refused");
    digestCase("/admin/secret.html", &good, 200, "An admin-role account on the admin route is accepted");
    digestCase("/digest/secret.html", &wrongRealm, 401, "A realm the server did not issue is challenged");
    digestCase("/digest/secret.html", &quotedPairCnonce, 200,
               "A quoted-pair escape in cnonce is unescaped and the response validates");
}

/*
    Replay: the nonce count must increase on every use of a nonce, and a failed attempt must not
    advance it
 */
static void testDigestReplay(void)
{
    Digest first = { .username = "alice", .ha1 = ALICE_HA1, .nc = "00000001" };
    Digest replay = { .username = "alice", .ha1 = ALICE_HA1, .nc = "00000001" };
    Digest second = { .username = "alice", .ha1 = ALICE_HA1, .nc = "00000002" };
    Digest skipFirst = { .username = "alice", .ha1 = ALICE_HA1, .nc = "00000005" };
    Digest badThenFirst = { .username = "alice", .ha1 = WENDY_HA1, .nc = "00000001" };
    char   nonce[512], opaque[128], algorithm[32];
    bool   sawBody;

    if (!challenge(HTTP, "/digest/secret.html", nonce, opaque, algorithm, sizeof(nonce))) {
        return;
    }
    teqi(digestGet(HTTP, "/digest/secret.html", &first, nonce, opaque, algorithm, &sawBody), 200);
    teqi(digestGet(HTTP, "/digest/secret.html", &replay, nonce, opaque, algorithm, &sawBody), 401,
         "Replaying a nonce count must be refused");
    ttrue(!sawBody);
    teqi(digestGet(HTTP, "/digest/secret.html", &second, nonce, opaque, algorithm, &sawBody), 200,
         "The next nonce count must be accepted");

    //  A fresh nonce must start at nc 1
    if (!challenge(HTTP, "/digest/secret.html", nonce, opaque, algorithm, sizeof(nonce))) {
        return;
    }
    teqi(digestGet(HTTP, "/digest/secret.html", &skipFirst, nonce, opaque, algorithm, &sawBody), 401,
         "A first use with a nonce count other than 1 must be refused");
    //  A failed attempt must not have consumed nc 1
    teqi(digestGet(HTTP, "/digest/secret.html", &badThenFirst, nonce, opaque, algorithm, &sawBody), 401);
    teqi(digestGet(HTTP, "/digest/secret.html", &first, nonce, opaque, algorithm, &sawBody), 200,
         "A failed attempt must not advance the nonce count");
}

/*
    The nonce table is bounded. Past the limit a new nonce is refused, while a nonce already in
    the table keeps working, so an attacker cannot evict legitimate sessions by minting nonces.
 */
static void testDigestNonceTableLimit(void)
{
    WebHost *host;
    Digest  users[4], again;
    cchar   *endpoint = "http://localhost:4857";
    char    *config, *ha1[4], *userList, nonces[4][512], opaque[128], algorithm[32], name[16], preimage[64];
    bool    sawBody;
    int     i, status;

    /*
        Use four accounts to fill the table within per-user limits. HA1 is H(user:Test Realm:password).
     */
    userList = sclone("");
    for (i = 0; i < 4; i++) {
        SFMT(name, "user%d", i);
        SFMT(preimage, "%s:Test Realm:password", name);
        ha1[i] = cryptGetSha256((cuchar*) preimage, slen(preimage));
        users[i] = (Digest) { .username = sclone(name), .ha1 = ha1[i], .nc = "00000001" };
        config = sfmt("%s%s: { password: 'SHA256:%s', role: 'admin' }, ", userList, name, ha1[i]);
        rFree(userList);
        userList = config;
    }
    config = sfmt("{ web: { documents: './site', listen: ['http://:4857'], "
                  "limits: { digest: 2 }, "
                  "auth: { realm: 'Test Realm', authType: 'digest', algorithm: 'SHA-256', "
                  "secret: 'test-secret-1234567890abcdef', "
                  "roles: { public: [], user: ['view'], admin: ['user'] }, "
                  "users: { %s } }, "
                  "routes: [ { match: '/digest/', authType: 'digest', role: 'user', handler: 'file' }, {} ] } }",
                  userList);
    host = startTestHost(config);
    rFree(config);
    rFree(userList);
    if (host) {
        //  Collect four nonces, then use them in order: the table holds two, the third fills it, the fourth is refused
        for (i = 0; i < 4; i++) {
            if (!challenge(endpoint, "/digest/secret.html", nonces[i], opaque, algorithm, sizeof(nonces[i]))) {
                break;
            }
        }
        if (i == 4) {
            for (i = 0; i < 3; i++) {
                status = digestGet(endpoint, "/digest/secret.html", &users[i], nonces[i], opaque, algorithm, &sawBody);
                teqi(status, 200, "Nonce %d must be accepted", i);
            }
            status = digestGet(endpoint, "/digest/secret.html", &users[3], nonces[3], opaque, algorithm, &sawBody);
            teqi(status, 401, "A new nonce past the table limit must be refused");
            ttrue(!sawBody);
            again = users[0];
            again.nc = "00000002";
            status = digestGet(endpoint, "/digest/secret.html", &again, nonces[0], opaque, algorithm, &sawBody);
            teqi(status, 200, "A nonce already in the table must keep working");
        }
        stopTestHost(host);
    }
    for (i = 0; i < 4; i++) {
        rFree((char*) users[i].username);
        rFree(ha1[i]);
    }
}

/*
    Basic: over TLS with a hand-built Authorization header
 */
static int basicGet(cchar *base, cchar *path, cchar *credentials, cchar *scheme, char *challenge, size_t size)
{
    Url   *up;
    cchar *header;
    char  url[256], *encoded;
    int   status;

    up = urlAlloc(0);
    if (credentials) {
        encoded = cryptEncode64Block((cuchar*) credentials, slen(credentials));
        status = urlFetch(up, "GET", SFMT(url, "%s%s", base, path), NULL, 0, "Authorization: %s %s\r\n", scheme, encoded);
        rFree(encoded);
    } else {
        status = urlFetch(up, "GET", SFMT(url, "%s%s", base, path), NULL, 0, NULL);
    }
    header = urlGetHeader(up, "WWW-Authenticate");
    scopy(challenge, size, header ? header : "");
    urlFree(up);
    return status;
}

static void testBasicRefusals(void)
{
    char challenge[512];
    int  status;

    status = basicGet(HTTPS, "/basic/secret.html", "ralph:password", "Basic", challenge, sizeof(challenge));
    teqi(status, 200, "Correct Basic credentials over TLS are accepted");

    status = basicGet(HTTPS, "/basic/secret.html", NULL, "Basic", challenge, sizeof(challenge));
    teqi(status, 401, "No credentials are challenged");
    tcontains(challenge, "Basic");

    status = basicGet(HTTPS, "/basic/secret.html", "nobody:password", "Basic", challenge, sizeof(challenge));
    teqi(status, 401, "An unknown user is challenged");

    status = basicGet(HTTPS, "/basic/secret.html", "ralph:wrong", "Basic", challenge, sizeof(challenge));
    teqi(status, 401, "A wrong password is challenged");

    status = basicGet(HTTPS, "/basic/secret.html", "ralphpassword", "Basic", challenge, sizeof(challenge));
    teqi(status, 401, "Credentials without a colon are challenged");

    status = basicGet(HTTPS, "/basic/secret.html", ":password", "Basic", challenge, sizeof(challenge));
    teqi(status, 401, "An empty username is challenged");

    status = basicGet(HTTPS, "/basic/secret.html", "ralph:password", "Bearer", challenge, sizeof(challenge));
    teqi(status, 401, "An unsupported scheme is challenged");

    status = basicGet(HTTP, "/basic/secret.html", "ralph:password", "Basic", challenge, sizeof(challenge));
    teqi(status, 403, "Basic over plain HTTP is refused when TLS is required");

    status = basicGet(HTTPS, "/digest/secret.html", "alice:password", "Basic", challenge, sizeof(challenge));
    teqi(status, 401, "Basic on a Digest route is challenged");
    tcontains(challenge, "Digest");
}

/*
    A Basic header whose payload is not base64 must be challenged, not crash or be half-decoded
 */
static void testBasicMalformedPayload(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/basic/secret.html", HTTPS), NULL, 0, "Authorization: Basic !!not-base64!!\r\n");
    teqi(status, 401, "A non-base64 payload is challenged");
    status = urlFetch(up, "GET", SFMT(url, "%s/basic/secret.html", HTTPS), NULL, 0, "Authorization: Basic\r\n");
    teqi(status, 401, "An empty payload is challenged");
    urlFree(up);
}

/*
    A route that requires Basic must not accept a Digest response: the stored digest hash would
    become usable credential material there
 */
static void testDigestOnBasicRoute(void)
{
    Digest good = { .username = "alice", .ha1 = ALICE_HA1 };
    char   nonce[512], opaque[128], algorithm[32];
    bool   sawBody;
    int    status;

    if (!challenge(HTTP, "/digest/secret.html", nonce, opaque, algorithm, sizeof(nonce))) {
        return;
    }
    //  Over HTTP the Basic challenge is answered as 403 when Basic requires TLS
    status = digestGet(HTTP, "/basic/secret.html", &good, nonce, opaque, algorithm, &sawBody);
    ttrue(status == 401 || status == 403, "Digest on a Basic route is refused, got %d", status);
    tfalse(sawBody, "Digest on a Basic route must not serve the protected document");
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, &HTTPS)) {
        testDigestRefusals();
        testDigestReplay();
        testDigestNonceTableLimit();
        testDigestOnBasicRoute();
        testBasicRefusals();
        testBasicMalformedPayload();
    }
    rFree(HTTP);
    rFree(HTTPS);
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
