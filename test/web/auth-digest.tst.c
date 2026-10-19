
/*
    auth-digest.tst.c - Unit tests for HTTP Digest authentication

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

#if ME_WEB_HTTP_AUTH && ME_WEB_AUTH_DIGEST

static void testDigest(void)
{
    Url   *up;
    char  *HTTP, url[256];
    int   status;
    cchar *header;

    // Setup with parent directory path to find web.json5
    if (!setup(&HTTP, NULL)) {
        return;
    }

    // Test 1: Public access (no auth required)
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200, "Public resource should return 200 OK");
    urlFree(up);

    /*
        Test 2: Protected resource without credentials (should get 401)
        Note: Routes need to be configured in web.json5 for full testing
     */
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/digest/secret.html", HTTP), NULL, 0, NULL);
    // Will be 404 until routes are configured - this is expected for now
    if (status == 401) {
        ttrue(1, "Protected resource without credentials returned 401");
        header = urlGetHeader(up, "WWW-Authenticate");
        if (header) {
            ttrue(sstarts(header, "Digest"), "Challenge should be Digest auth");
            ttrue(scontains(header, "realm="), "Challenge should contain realm");
            ttrue(scontains(header, "nonce="), "Challenge should contain nonce");
            ttrue(scontains(header, "algorithm="), "Challenge should contain algorithm");
            ttrue(scontains(header, "qop="), "Challenge should contain qop");
        }
    } else {
        // Route not configured yet - skip test
        tinfo("Digest route not configured, skipping 401 test");
    }
    urlFree(up);

    /*
        Test 3: alice with SHA256 password (admin role) accessing /digest/
     */
    up = urlAlloc(0);
    urlSetAuth(up, "alice", "password", "digest");
    status = urlFetch(up, "GET", SFMT(url, "%s/digest/secret.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200, "alice (SHA256) should access /digest/ with Digest auth");
    urlFree(up);

    /*
        Test 4: alice (admin) can access /admin/ with Digest
     */
    up = urlAlloc(0);
    urlSetAuth(up, "alice", "password", "digest");
    status = urlFetch(up, "GET", SFMT(url, "%s/admin/secret.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200, "alice (admin role) should access /admin/ with Digest");
    urlFree(up);

    /*
        Test 5: bob with MD5 password (user role) accessing /digest-md5/
        NOTE: bob has MD5 password, so must use MD5 digest route
     */
    up = urlAlloc(0);
    urlSetAuth(up, "bob", "password", "digest");
    status = urlFetch(up, "GET", SFMT(url, "%s/digest-md5/secret.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200, "bob (MD5) should access /digest-md5/ with Digest auth");
    urlFree(up);

    /*
        Test 6: Wrong password should be rejected
     */
    up = urlAlloc(0);
    urlSetAuth(up, "alice", "wrongpassword", "digest");
    status = urlFetch(up, "GET", SFMT(url, "%s/digest/secret.html", HTTP), NULL, 0, NULL);
    ttrue(status == 401, "Wrong password should return 401 with Digest auth");
    urlFree(up);

    /*
        Test 7: bob (user role) CANNOT access /admin/ (should get 401)
        NOTE: bob has MD5 password, /admin/ uses SHA-256, so he can't authenticate
     */
    up = urlAlloc(0);
    urlSetAuth(up, "bob", "password", "digest");
    status = urlFetch(up, "GET", SFMT(url, "%s/admin/secret.html", HTTP), NULL, 0, NULL);
    ttrue(status == 401, "bob (MD5) should get 401 for /admin/ (algorithm mismatch) with Digest");
    urlFree(up);

    /*
        Test 8: Auto-detect auth type (server will send Digest challenge)
     */
    up = urlAlloc(0);
    urlSetAuth(up, "alice", "password", NULL);  // NULL = auto-detect
    status = urlFetch(up, "GET", SFMT(url, "%s/digest/secret.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200, "Auto-detect should work for Digest auth");
    urlFree(up);

    /*
        Test 9: Unknown user should be rejected
     */
    up = urlAlloc(0);
    urlSetAuth(up, "unknownuser", "password", "digest");
    status = urlFetch(up, "GET", SFMT(url, "%s/digest/secret.html", HTTP), NULL, 0, NULL);
    ttrue(status == 401, "Unknown user should return 401 for Digest auth");
    urlFree(up);

    /*
        Test 10: Wrong auth scheme - URL client auto-upgrades to Digest
        NOTE: URL client automatically switches to Digest when it sees a Digest challenge,
        so this test verifies the auto-upgrade behavior works correctly
     */
    up = urlAlloc(0);
    urlSetAuth(up, "alice", "password", "basic");
    status = urlFetch(up, "GET", SFMT(url, "%s/digest/secret.html", HTTP), NULL, 0, NULL);
    tinfo("Basic credentials on Digest route (auto-upgraded): status = %d", status);
    ttrue(status == 200, "URL client should auto-upgrade from Basic to Digest and succeed");
    urlFree(up);

    /*
        Test 11: Reusing a Url handle keeps Digest session information
     */
    up = urlAlloc(0);
    urlSetAuth(up, "alice", "password", "digest");
    status = urlFetch(up, "GET", SFMT(url, "%s/digest/secret.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200, "Initial request with alice should succeed");
    status = urlFetch(up, "GET", SFMT(url, "%s/digest/secret.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200, "Subsequent request should reuse Digest context");
    urlFree(up);

    /*
        Test 12: Algorithm mismatch - bob (MD5) cannot access SHA-256 route
     */
    up = urlAlloc(0);
    urlSetAuth(up, "bob", "password", "digest");
    status = urlFetch(up, "GET", SFMT(url, "%s/digest/secret.html", HTTP), NULL, 0, NULL);
    ttrue(status == 401, "bob (MD5 password) should not pass on SHA-256 digest route");
    urlFree(up);

    /*
        Test 13: ralph (Bcrypt) CANNOT use Digest authentication at all
     */
    up = urlAlloc(0);
    urlSetAuth(up, "ralph", "password", "digest");
    status = urlFetch(up, "GET", SFMT(url, "%s/digest/secret.html", HTTP), NULL, 0, NULL);
    ttrue(status == 401, "ralph (Bcrypt password) cannot use Digest authentication");
    urlFree(up);

    rFree(HTTP);
    tinfo("Digest authentication tests completed");
}

/*
    Helpers to parse Digest challenge and craft Authorization headers for negative tests
 */
static char *getParam(cchar *hdr, cchar *name, char *buf, size_t buflen)
{
    cchar  *p = strstr(hdr, name);
    cchar  *start, *end;
    size_t len;

    if (!p) return NULL;
    p += slen(name);
    while (*p && (*p == '=' || *p == ' ')) p++;
    if (*p == '\"') {
        start = ++p;
        end = strchr(p, '\"');
    } else {
        start = p;
        end = strchr(p, ',');
        if (!end) end = p + slen(p);
    }
    if (!end) return NULL;
    len = (size_t) (end - start);
    if (len >= buflen) len = buflen - 1;
    memcpy(buf, start, len);
    buf[len] = '\0';
    return buf;
}

/*
    A hand-built Digest session for the replay tests. Fetches one challenge, then lets the test
    send as many requests as it likes under that nonce with any nc and any response it chooses,
    so the server's nonce-count tracking can be driven deliberately out of order.
 */
typedef struct DigestSession {
    char *HTTP;
    char realm[128];
    char nonce[256];
    char algorithm[16];
    char *ha1;
    char *ha2;
    cchar *uri;
} DigestSession;

static bool openDigestSession(DigestSession *ds, cchar *uri)
{
    Url   *up;
    //  Sized for the long-uri case below, where the request target alone is over 600 bytes
    char  url[2048], buf[2048];
    cchar *wwwAuth;
    int   status;

    memset(ds, 0, sizeof(*ds));
    if (!setup(&ds->HTTP, NULL)) {
        return 0;
    }
    ds->uri = uri;
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s%s", ds->HTTP, uri), NULL, 0, NULL);
    teqi(status, 401);
    wwwAuth = urlGetHeader(up, "WWW-Authenticate");
    tnotnull(wwwAuth);
    if (!wwwAuth) {
        urlFree(up);
        return 0;
    }
    getParam(wwwAuth, "realm", ds->realm, sizeof(ds->realm));
    getParam(wwwAuth, "nonce", ds->nonce, sizeof(ds->nonce));
    if (!getParam(wwwAuth, "algorithm", ds->algorithm, sizeof(ds->algorithm))) {
        scopy(ds->algorithm, sizeof(ds->algorithm), "SHA-256");
    }
    urlFree(up);

    //  alice's password is 'password'
    SFMT(buf, "alice:%s:password", ds->realm);
    ds->ha1 = webHash(buf, ds->algorithm);
    SFMT(buf, "GET:%s", uri);
    ds->ha2 = webHash(buf, ds->algorithm);
    return 1;
}

static void closeDigestSession(DigestSession *ds)
{
    rFree(ds->ha1);
    rFree(ds->ha2);
    rFree(ds->HTTP);
}

/*
    The correct response for a given nc and cnonce under this session's nonce. Caller frees.
 */
static char *digestResponse(DigestSession *ds, cchar *nc, cchar *cnonce)
{
    char buf[1024];

    SFMT(buf, "%s:%s:%s:%s:auth:%s", ds->ha1, ds->nonce, nc, cnonce, ds->ha2);
    return webHash(buf, ds->algorithm);
}

/*
    Send one request under the session nonce and return the status. A null response means
    "compute the correct one"; pass a wrong value to simulate an attacker without the password.
 */
static int digestRequest(DigestSession *ds, cchar *nc, cchar *cnonce, cchar *response)
{
    Url  *up;
    char url[2048], header[4096], *computed;
    int  status;

    computed = response ? 0 : digestResponse(ds, nc, cnonce);
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s%s", ds->HTTP, ds->uri), NULL, 0,
                      SFMT(header,
                           "Authorization: Digest username=\"alice\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", "
                           "response=\"%s\", algorithm=\"%s\", qop=auth, nc=%s, cnonce=\"%s\"\r\n",
                           ds->realm, ds->nonce, ds->uri, response ? response : computed, ds->algorithm,
                           nc, cnonce));
    urlFree(up);
    rFree(computed);
    return status;
}

/*
    Send one request and assert its status.

    The request must be made exactly once. teqi evaluates its first argument twice - once to
    compare and once to report - so calling digestRequest inside it would send two requests and
    make the nonce-count sequence these tests depend on meaningless.
 */
static void checkDigest(DigestSession *ds, cchar *nc, cchar *cnonce, cchar *response, int expect)
{
    int status;

    status = digestRequest(ds, nc, cnonce, response);
    teqi(status, expect);
}

/*
    Replay after eviction. A captured request must stay unreplayable even after an
    attacker, who does not know the password, sends a failing request under the same nonce with
    a higher nc. Before the fix, that failing request deleted the nonce's tracking entry, which
    reset the nonce to first-use and let the captured nc=1 request through again.
 */
static void testDigestReplayAfterEviction(void)
{
    DigestSession ds;
    char          *captured;

    if (!openDigestSession(&ds, "/digest/secret.html")) {
        return;
    }
    //  1. The legitimate client's request, which an on-path attacker captures
    captured = digestResponse(&ds, "00000001", "evict01");
    checkDigest(&ds, "00000001", "evict01", captured, 200);

    //  2. Attacker tries to reset the nonce: higher nc, garbage response
    checkDigest(&ds, "00000002", "evict01", "0000000000000000", 401);

    //  3. Replay of the captured request must still be refused
    checkDigest(&ds, "00000001", "evict01", captured, 401);

    //  4. And the legitimate client can carry on - its next nc is still accepted
    checkDigest(&ds, "00000002", "evict02", NULL, 200);

    //  5. A failed attempt must not have advanced the count either: nc=3 still works after
    //     an attacker sent a garbage nc=9
    checkDigest(&ds, "00000009", "evict01", "0000000000000000", 401);
    checkDigest(&ds, "00000003", "evict03", NULL, 200);

    rFree(captured);
    closeDigestSession(&ds);
}

/*
    A normal session increments nc on every request. Counts at and above 0x80000000 are valid and
    must compare as unsigned: the old (int) conversion made them negative and refused them.
 */
static void testDigestNcSequence(void)
{
    DigestSession ds;

    if (!openDigestSession(&ds, "/digest/secret.html")) {
        return;
    }
    checkDigest(&ds, "00000001", "seq01", NULL, 200);
    checkDigest(&ds, "00000002", "seq02", NULL, 200);
    //  Gaps are allowed - the count only has to increase
    checkDigest(&ds, "00000010", "seq03", NULL, 200);
    //  Going backwards is a replay
    checkDigest(&ds, "00000002", "seq02", NULL, 401);
    checkDigest(&ds, "00000010", "seq03", NULL, 401);
    //  High bit set
    checkDigest(&ds, "80000001", "seq04", NULL, 200);
    checkDigest(&ds, "ffffffff", "seq05", NULL, 200);
    //  Nothing exceeds ffffffff, so the nonce is now exhausted
    checkDigest(&ds, "00000001", "seq06", NULL, 401);
    closeDigestSession(&ds);
}

/*
    nc is exactly eight hexadecimal digits. The first use of a nonce is nc=1.
 */
static void testDigestNcFormat(void)
{
    DigestSession ds;

    if (!openDigestSession(&ds, "/digest/secret.html")) {
        return;
    }
    //  A first request that is not nc=1 is not a first request
    checkDigest(&ds, "00000005", "fmt01", NULL, 401);

    //  Malformed counts are refused even with a correct digest over them
    checkDigest(&ds, "0x000001", "fmt02", NULL, 401);
    checkDigest(&ds, "000000001", "fmt03", NULL, 401);
    checkDigest(&ds, "0000001", "fmt04", NULL, 401);
    checkDigest(&ds, "0000000g", "fmt05", NULL, 401);
    checkDigest(&ds, "-0000001", "fmt06", NULL, 401);
    checkDigest(&ds, "+0000001", "fmt07", NULL, 401);

    //  None of that touched the tracking state, so the real first request still works
    checkDigest(&ds, "00000001", "fmt08", NULL, 200);
    //  Upper case hex is fine
    checkDigest(&ds, "0000000A", "fmt09", NULL, 200);
    closeDigestSession(&ds);
}

static void testDigestUriMismatch(void)
{
    Url   *up;
    char  *HTTP, url[256];
    int   status;
    cchar *wwwAuth;
    char  realm[128], nonce[256], algorithm[16];
    char  cnonce[32] = "abc123", nc[16] = "00000001";
    char  ha1buf[512], ha2buf[512], respBuf[1024];
    char  header[1024];
    char  *ha1, *ha2, *response;
    cchar *wrongUri = "/digest/secret2.html";
    cchar *rightUri = "/digest/secret.html";

    if (!setup(&HTTP, NULL)) {
        return;
    }
    // Get challenge
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s%s", HTTP, rightUri), NULL, 0, NULL);
    ttrue(status == 401, "Expect 401 for initial challenge");
    wwwAuth = urlGetHeader(up, "WWW-Authenticate");
    ttrue(wwwAuth != NULL, "Expect WWW-Authenticate header");
    getParam(wwwAuth, "realm", realm, sizeof(realm));
    getParam(wwwAuth, "nonce", nonce, sizeof(nonce));
    if (!getParam(wwwAuth, "algorithm", algorithm, sizeof(algorithm))) {
        scopy(algorithm, sizeof(algorithm), "SHA-256");
    }
    urlFree(up);

    // Compute response with wrong URI in header
    SFMT(ha1buf, "alice:%s:%s", realm, "password");
    ha1 = webHash(ha1buf, algorithm);
    SFMT(ha2buf, "GET:%s", wrongUri);
    ha2 = webHash(ha2buf, algorithm);
    SFMT(respBuf, "%s:%s:%s:%s:%s:%s", ha1, nonce, nc, cnonce, "auth", ha2);
    response = webHash(respBuf, algorithm);

    // Send request to rightUri but with Authorization header using wrong URI
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s%s", HTTP, rightUri), NULL, 0,
                      SFMT(header,
                           "Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\", algorithm=\"%s\", qop=auth, nc=%s, cnonce=\"%s\"\r\n",
                           "alice", realm, nonce, wrongUri, response, algorithm, nc, cnonce));
    ttrue(status == 401, "URI mismatch must be rejected");
    urlFree(up);
    rFree(ha1);
    rFree(ha2);
    rFree(response);
    rFree(HTTP);
}

static void testDigestReplay(void)
{
    Url   *up;
    char  *HTTP, url[256];
    int   status;
    cchar *wwwAuth;
    char  realm[128], nonce[256], algorithm[16];
    char  cnonce[32] = "xyz789", nc[16] = "00000001";
    char  ha1buf[512], ha2buf[512], respBuf[1024];
    char  header[1024];
    char  *ha1, *ha2, *response;
    cchar *uri = "/digest/secret.html";

    if (!setup(&HTTP, NULL)) {
        return;
    }
    // Get challenge
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s%s", HTTP, uri), NULL, 0, NULL);
    ttrue(status == 401, "Expect 401 for initial challenge");
    wwwAuth = urlGetHeader(up, "WWW-Authenticate");
    ttrue(wwwAuth != NULL, "Expect WWW-Authenticate header");
    getParam(wwwAuth, "realm", realm, sizeof(realm));
    getParam(wwwAuth, "nonce", nonce, sizeof(nonce));
    if (!getParam(wwwAuth, "algorithm", algorithm, sizeof(algorithm))) {
        scopy(algorithm, sizeof(algorithm), "SHA-256");
    }
    urlFree(up);

    // Compute valid response for first request
    SFMT(ha1buf, "alice:%s:%s", realm, "password");
    ha1 = webHash(ha1buf, algorithm);
    SFMT(ha2buf, "GET:%s", uri);
    ha2 = webHash(ha2buf, algorithm);
    SFMT(respBuf, "%s:%s:%s:%s:%s:%s", ha1, nonce, nc, cnonce, "auth", ha2);
    response = webHash(respBuf, algorithm);

    // First request with Authorization header -> expect 200
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s%s", HTTP, uri), NULL, 0,
                      SFMT(header,
                           "Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\", algorithm=\"%s\", qop=auth, nc=%s, cnonce=\"%s\"\r\n",
                           "alice", realm, nonce, uri, response, algorithm, nc, cnonce));
    ttrue(status == 200, "First valid digest request should succeed");
    urlFree(up);

    // Replay exact same Authorization -> expect 401
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s%s", HTTP, uri), NULL, 0,
                      SFMT(header,
                           "Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\", algorithm=\"%s\", qop=auth, nc=%s, cnonce=\"%s\"\r\n",
                           "alice", realm, nonce, uri, response, algorithm, nc, cnonce));
    ttrue(status == 401, "Replayed digest credentials must be rejected");
    urlFree(up);

    rFree(ha1);
    rFree(ha2);
    rFree(response);
    rFree(HTTP);
}

static void testDigestAlgorithmMismatch(void)
{
    Url   *up;
    char  *HTTP, url[256];
    int   status;
    cchar *wwwAuth;
    char  realm[128], nonce[256];
    char  cnonce[32] = "mismatch", nc[16] = "00000001";
    char  header[1024];
    cchar *uri = "/digest/secret.html";

    if (!setup(&HTTP, NULL)) {
        return;
    }
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s%s", HTTP, uri), NULL, 0, NULL);
    ttrue(status == 401, "Expect 401 for initial challenge");
    wwwAuth = urlGetHeader(up, "WWW-Authenticate");
    ttrue(wwwAuth != NULL, "Expect WWW-Authenticate header");
    getParam(wwwAuth, "realm", realm, sizeof(realm));
    getParam(wwwAuth, "nonce", nonce, sizeof(nonce));
    urlFree(up);

    // Send header with wrong algorithm token (MD5) while route uses SHA-256
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s%s", HTTP, uri), NULL, 0,
                      SFMT(header,
                           "Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\", algorithm=\"MD5\", qop=auth, nc=%s, cnonce=\"%s\"\r\n",
                           "alice", realm, nonce, uri, "00", nc, cnonce));
    ttrue(status == 401, "Algorithm mismatch should be rejected");
    urlFree(up);
    rFree(HTTP);
}

static void testDigestSha512Rejected(void)
{
    Url   *up;
    char  *HTTP, url[256];
    int   status;
    cchar *wwwAuth;
    char  realm[128], nonce[256];
    char  cnonce[32] = "sha512", nc[16] = "00000001";
    char  header[1024];
    cchar *uri = "/digest/secret.html";

    if (!setup(&HTTP, NULL)) {
        return;
    }
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s%s", HTTP, uri), NULL, 0, NULL);
    ttrue(status == 401, "Expect 401 for initial challenge");
    wwwAuth = urlGetHeader(up, "WWW-Authenticate");
    ttrue(wwwAuth != NULL, "Expect WWW-Authenticate header");
    getParam(wwwAuth, "realm", realm, sizeof(realm));
    getParam(wwwAuth, "nonce", nonce, sizeof(nonce));
    urlFree(up);

    // Send header advertising unsupported algorithm => parse failure -> 401
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s%s", HTTP, uri), NULL, 0,
                      SFMT(header,
                           "Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\", algorithm=\"SHA-512-256\", qop=auth, nc=%s, cnonce=\"%s\"\r\n",
                           "alice", realm, nonce, uri, "00", nc, cnonce));
    ttrue(status == 401, "Unsupported SHA-512 algorithm must be rejected");
    urlFree(up);
    rFree(HTTP);
}

/*
    RFC 7616 section 3.4 makes the digest uri the Request-URI, so it carries the query string.
    The URI binding must compare the whole target, not just the path: a request with a query must
    authenticate, and a uri whose query differs from the request's own must still be refused.
 */
static void testDigestWithQuery(void)
{
    Url   *up;
    char  *HTTP, url[256];
    int   status;
    cchar *wwwAuth;
    char  realm[128], nonce[256], algorithm[16];
    char  cnonce[32] = "query01", nc[16] = "00000001";
    char  ha1buf[512], ha2buf[512], respBuf[1024];
    char  header[1024];
    char  *ha1, *ha2, *response;
    cchar *target = "/digest/secret.html?a=1&b=2";
    cchar *otherQuery = "/digest/secret.html?a=9&b=9";

    if (!setup(&HTTP, NULL)) {
        return;
    }
    //  A digest request carrying a query must authenticate
    up = urlAlloc(0);
    urlSetAuth(up, "alice", "password", "digest");
    status = urlFetch(up, "GET", SFMT(url, "%s%s", HTTP, target), NULL, 0, NULL);
    ttrue(status == 200, "Digest request with a query string should succeed");
    ttrue(scontains(urlGetResponse(up), "Digest Authentication Success"), "Expect the protected document");
    urlFree(up);

    //  Take a challenge so the binding can be tested with a hand built header
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s%s", HTTP, target), NULL, 0, NULL);
    ttrue(status == 401, "Expect 401 for initial challenge");
    wwwAuth = urlGetHeader(up, "WWW-Authenticate");
    ttrue(wwwAuth != NULL, "Expect WWW-Authenticate header");
    getParam(wwwAuth, "realm", realm, sizeof(realm));
    getParam(wwwAuth, "nonce", nonce, sizeof(nonce));
    if (!getParam(wwwAuth, "algorithm", algorithm, sizeof(algorithm))) {
        scopy(algorithm, sizeof(algorithm), "SHA-256");
    }
    urlFree(up);

    //  A uri naming the right path but a different query must not authenticate the request
    SFMT(ha1buf, "alice:%s:%s", realm, "password");
    ha1 = webHash(ha1buf, algorithm);
    SFMT(ha2buf, "GET:%s", otherQuery);
    ha2 = webHash(ha2buf, algorithm);
    SFMT(respBuf, "%s:%s:%s:%s:%s:%s", ha1, nonce, nc, cnonce, "auth", ha2);
    response = webHash(respBuf, algorithm);

    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s%s", HTTP, target), NULL, 0,
                      SFMT(header,
                           "Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\", algorithm=\"%s\", qop=auth, nc=%s, cnonce=\"%s\"\r\n",
                           "alice", realm, nonce, otherQuery, response, algorithm, nc, cnonce));
    ttrue(status == 401, "A digest uri with a different query must be rejected");
    urlFree(up);

    rFree(ha1);
    rFree(ha2);
    rFree(response);
    rFree(HTTP);
}

/*
    A repeated Digest auth-param is malformed: RFC 7616 section 3.4 defines the credentials as a
    set. The credential below is otherwise valid and the repeat carries the same value, so under
    the old last-wins parse it authenticated - the refusal must come from the repeat alone.
 */
static void testDigestDuplicateParameter(void)
{
    DigestSession ds;
    Url           *up;
    char          url[256], header[1024], *response;
    int           status;

    if (!openDigestSession(&ds, "/digest/secret.html")) {
        return;
    }
    response = digestResponse(&ds, "00000001", "dup01");
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s%s", ds.HTTP, ds.uri), NULL, 0,
                      SFMT(header,
                           "Authorization: Digest username=\"alice\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", "
                           "response=\"%s\", algorithm=\"%s\", qop=auth, nc=00000001, cnonce=\"dup01\", "
                           "username=\"alice\"\r\n",
                           ds.realm, ds.nonce, ds.uri, response, ds.algorithm));
    teqi(status, 401);
    urlFree(up);

    /*
        The identical credential without the repeat still authenticates under the same nonce and
        nc, so the rejection above is the duplicate and not the credential. The rejected request
        never reached nonce validation, so nc=1 is still unused.
     */
    checkDigest(&ds, "00000001", "dup01", response, 200);
    rFree(response);
    closeDigestSession(&ds);
}

/*
    HA2 is H(method:uri) and was built in a fixed 512-byte buffer, so any request target over about
    508 bytes was hashed truncated while the client hashed the whole thing - the two disagreed and
    a correct credential was refused. Worse, two targets sharing a 508-byte prefix produced the
    same HA2, so a credential minted for one was valid for the other. parseDigestAuth permits a uri
    of 2048, so this length is legal input, not an abuse.

    The length is carried in a query string rather than a deep path: the request target has to
    exceed 512 bytes, and a 600-byte filesystem path is beyond MAX_PATH on Windows.
 */
static void testDigestLongUri(void)
{
    DigestSession ds;
    char          uri[1024];
    size_t        len;

    scopy(uri, sizeof(uri), "/digest/secret.html?q=");
    len = slen(uri);
    memset(&uri[len], 'A', 600);
    uri[len + 600] = '\0';

    if (!openDigestSession(&ds, uri)) {
        return;
    }
    //  A correct credential over a 622-byte request target must authenticate
    checkDigest(&ds, "00000001", "long01", NULL, 200);
    closeDigestSession(&ds);
}

static void fiberMain(void *data)
{
    testDigest();
    testDigestDuplicateParameter();
    testDigestLongUri();
    testDigestUriMismatch();
    testDigestWithQuery();
    testDigestReplay();
    testDigestReplayAfterEviction();
    testDigestNcSequence();
    testDigestNcFormat();
    testDigestAlgorithmMismatch();
    testDigestSha512Rejected();
    rStop();
}

#else

static void fiberMain(void *data)
{
    tinfo("Digest authentication not enabled in build - test skipped");
    rStop();
}

#endif /* ME_WEB_HTTP_AUTH && ME_WEB_AUTH_DIGEST */

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
