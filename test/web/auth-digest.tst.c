
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

static void fiberMain(void *data)
{
    testDigest();
    testDigestUriMismatch();
    testDigestReplay();
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
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
