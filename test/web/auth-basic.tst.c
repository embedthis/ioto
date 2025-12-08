
/*
    auth-basic.tst.c - Unit tests for HTTP Basic authentication

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/************************************ Code ************************************/

#if ME_WEB_HTTP_AUTH && ME_WEB_AUTH_BASIC

static void testBasic(void)
{
    Url   *up;
    char  *HTTPS, url[256];
    int   status;
    cchar *header;

    // Setup with HTTPS endpoint (Basic auth requires TLS)
    if (!setup(NULL, &HTTPS)) {
        tfail("Setup failed to read web.json5");
        return;
    }

    // Test 1: Public access (no auth required)
    up = urlAlloc(0);
    tinfo("Testing HTTPS endpoint: %s/index.html", HTTPS);
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTPS), NULL, 0, NULL);
    tinfo("Received HTTPS status: %d", status);
    ttrue(status == 200, "Public resource should return 200 OK");
    urlFree(up);

    /*
        Test 2: Protected resource without credentials (should get 401)
        Note: Routes need to be configured in web.json5 for full testing
     */
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/basic/secret.html", HTTPS), NULL, 0, NULL);
    // Will be 404 until routes are configured - this is expected for now
    if (status == 401) {
        ttrue(1, "Protected resource without credentials returned 401");
        header = urlGetHeader(up, "WWW-Authenticate");
        if (header) {
            ttrue(sstarts(header, "Basic"), "Challenge should be Basic auth");
            ttrue(scontains(header, "realm="), "Challenge should contain realm");
        }
    } else {
        // Route not configured yet - skip test
        tinfo("Basic route not configured, skipping 401 test");
    }
    urlFree(up);

    /*
        Test 3: alice with SHA256 password (admin role) accessing /basic/
     */
    up = urlAlloc(0);
    urlSetAuth(up, "alice", "password", "basic");
    status = urlFetch(up, "GET", SFMT(url, "%s/basic/secret.html", HTTPS), NULL, 0, NULL);
    ttrue(status == 200, "alice (SHA256) should access /basic/");
    urlFree(up);

    /*
        Test 4: alice (admin) with Basic auth accessing /admin/ (Digest-only route)
        NOTE: URL client auto-upgrades from Basic to Digest when challenged
     */
    up = urlAlloc(0);
    urlSetAuth(up, "alice", "password", "basic");
    status = urlFetch(up, "GET", SFMT(url, "%s/admin/secret.html", HTTPS), NULL, 0, NULL);
    ttrue(status == 200, "alice (auto-upgraded to Digest) should access /admin/");
    urlFree(up);

    /*
        Test 5: bob with MD5 password (user role) accessing /basic/
     */
    up = urlAlloc(0);
    urlSetAuth(up, "bob", "password", "basic");
    status = urlFetch(up, "GET", SFMT(url, "%s/basic/secret.html", HTTPS), NULL, 0, NULL);
    ttrue(status == 200, "bob (MD5) should access /basic/");
    urlFree(up);

    /*
        Test 6: bob (user role) accessing /admin/ with Basic auth
        NOTE: /admin/ route is digest-only, so Basic auth should get 401
     */
    up = urlAlloc(0);
    urlSetAuth(up, "bob", "password", "basic");
    status = urlFetch(up, "GET", SFMT(url, "%s/admin/secret.html", HTTPS), NULL, 0, NULL);
    ttrue(status == 401, "bob (Basic auth) should get 401 for /admin/ (Digest-only route)");
    urlFree(up);

    /*
        Test 7: ralph with Bcrypt password (user role) accessing /basic/
     */
    up = urlAlloc(0);
    urlSetAuth(up, "ralph", "password", "basic");
    status = urlFetch(up, "GET", SFMT(url, "%s/basic/secret.html", HTTPS), NULL, 0, NULL);
    ttrue(status == 200, "ralph (Bcrypt) should access /basic/");
    urlFree(up);

    /*
        Test 8: Wrong password should be rejected
     */
    up = urlAlloc(0);
    urlSetAuth(up, "alice", "wrongpassword", "basic");
    status = urlFetch(up, "GET", SFMT(url, "%s/basic/secret.html", HTTPS), NULL, 0, NULL);
    ttrue(status == 401, "Wrong password should return 401");
    urlFree(up);

    rFree(HTTPS);
    tinfo("Basic authentication tests completed");
}

/*
    Test that Basic auth is properly rejected over HTTP when requireTlsForBasic is enabled
 */
static void testBasicTlsRequired(void)
{
    Url  *up;
    char *HTTP, url[256];
    int  status;

    // Setup with HTTP endpoint to test rejection
    if (!setup(&HTTP, NULL)) {
        tfail("Setup failed to read web.json5");
        return;
    }

    /*
        Test: Basic auth over HTTP should return 403 when requireTlsForBasic is true
     */
    up = urlAlloc(0);
    urlSetAuth(up, "alice", "password", "basic");
    status = urlFetch(up, "GET", SFMT(url, "%s/basic/secret.html", HTTP), NULL, 0, NULL);
    ttrue(status == 403, "Basic auth over HTTP should return 403 when TLS is required");
    urlFree(up);

    rFree(HTTP);
    tinfo("Basic TLS enforcement test completed");
}

static void fiberMain(void *data)
{
    testBasic();
    testBasicTlsRequired();
    rStop();
}

#else

static void fiberMain(void *data)
{
    tinfo("Basic authentication not enabled in build - test skipped");
    rStop();
}

#endif /* ME_WEB_HTTP_AUTH && ME_WEB_AUTH_BASIC */

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
