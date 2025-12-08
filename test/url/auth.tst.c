/*
    auth.tst.c - Integration tests for HTTP authentication with real server

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

#if URL_AUTH

/*********************************** Locals ***********************************/

static char    *HTTP;
static char    *HTTPS;

/************************************ Code ************************************/

/*
    Test Basic authentication with correct credentials
 */
static void testBasicAuthSuccess()
{
    Url     *up;
    char    url[128];
    int     status;
    cchar   *response;

    up = urlAlloc(0);
    urlSetAuth(up, "bob", "password", "basic");
    status = urlFetch(up, "GET", SFMT(url, "%s/basic/secret.html", HTTP), NULL, 0, NULL);

    tinfo("Status: %d", status);
    if (status < 0) {
        tinfo("Error: %s", rGetError(status));
    }
    ttrue(status == 200);
    response = urlGetResponse(up);
    ttrue(response != NULL);
    if (response) {
        tinfo("Response length: %d", (int) slen(response));
        ttrue(scontains(response, "Basic Authentication Success"));
    }

    urlFree(up);
}

/*
    Test Basic authentication with incorrect credentials
 */
static void testBasicAuthFailure()
{
    Url     *up;
    char    url[128];
    int     status;

    up = urlAlloc(0);
    urlSetAuth(up, "bob", "wrongpassword", "basic");
    status = urlFetch(up, "GET", SFMT(url, "%s/basic/secret.html", HTTP), NULL, 0, NULL);

    // Should fail with 401 Unauthorized
    ttrue(status == 401);

    urlFree(up);
}

/*
    Test Digest authentication with SHA-256 algorithm
 */
static void testDigestSHA256Success()
{
    Url     *up;
    char    url[128];
    int     status;
    cchar   *response;

    up = urlAlloc(0);
    urlSetAuth(up, "alice", "password", "digest");
    status = urlFetch(up, "GET", SFMT(url, "%s/digest/secret.html", HTTP), NULL, 0, NULL);

    ttrue(status == 200);
    response = urlGetResponse(up);
    ttrue(response != NULL);
    ttrue(scontains(response, "Digest Authentication Success"));

    urlFree(up);
}

/*
    Test Digest authentication with MD5 algorithm (legacy)
 */
static void testDigestMD5Success()
{
    Url     *up;
    char    url[128];
    int     status;
    cchar   *response;

    up = urlAlloc(0);
    urlSetAuth(up, "bob", "password", "digest");
    status = urlFetch(up, "GET", SFMT(url, "%s/digest-md5/secret.html", HTTP), NULL, 0, NULL);

    ttrue(status == 200);
    response = urlGetResponse(up);
    ttrue(response != NULL);

    urlFree(up);
}

/*
    Test Digest authentication with incorrect credentials
 */
static void testDigestAuthFailure()
{
    Url     *up;
    char    url[128];
    int     status;

    up = urlAlloc(0);
    urlSetAuth(up, "alice", "wrongpassword", "digest");
    status = urlFetch(up, "GET", SFMT(url, "%s/digest/secret.html", HTTP), NULL, 0, NULL);

    // Should fail with 401 Unauthorized
    ttrue(status == 401);

    urlFree(up);
}

/*
    Test authentication with auto-detection (NULL authType)
 */
static void testAuthAutoDetect()
{
    Url     *up;
    char    url[128];
    int     status;
    cchar   *response;

    // Test auto-detection with Basic auth endpoint
    up = urlAlloc(0);
    urlSetAuth(up, "bob", "password", NULL);  // Auto-detect
    status = urlFetch(up, "GET", SFMT(url, "%s/basic/secret.html", HTTP), NULL, 0, NULL);

    ttrue(status == 200);
    response = urlGetResponse(up);
    ttrue(response != NULL);
    ttrue(scontains(response, "Basic Authentication Success"));

    urlFree(up);
}

/*
    Test role-based access control
 */
static void testRoleBasedAccess()
{
    Url     *up;
    char    url[128];
    int     status;

    // Bob (user role) should not be able to access admin area
    up = urlAlloc(0);
    urlSetAuth(up, "bob", "password", "digest");
    status = urlFetch(up, "GET", SFMT(url, "%s/admin/secret.html", HTTP), NULL, 0, NULL);

    // Should fail with 401 Unauthorized (MD5 password incompatible with SHA-256 route)
    tinfo("Bob accessing admin: status=%d (expected 401)", status);
    ttrue(status == 401);

    urlFree(up);

    // Alice (admin role) should be able to access admin area
    up = urlAlloc(0);
    urlSetAuth(up, "alice", "password", "digest");
    status = urlFetch(up, "GET", SFMT(url, "%s/admin/secret.html", HTTP), NULL, 0, NULL);

    tinfo("Alice accessing admin: status=%d (expected 200)", status);
    ttrue(status == 200);

    urlFree(up);
}

/*
    Test nonce reuse optimization (subsequent requests should reuse nonce)
 */
static void testNonceReuse()
{
    Url     *up;
    char    url[128];
    int     status;
    cchar   *response;

    up = urlAlloc(0);
    fprintf(stderr, "AT %d\n", __LINE__);
    urlSetAuth(up, "alice", "password", "digest");

    // First request - performs 401 challenge
    status = urlFetch(up, "GET", SFMT(url, "%s/digest/secret.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200);
    fprintf(stderr, "AT %d\n", __LINE__);

    // Second request - should reuse nonce (no 401)
    status = urlFetch(up, "GET", SFMT(url, "%s/digest/secret.html", HTTP), NULL, 0, NULL);
    fprintf(stderr, "AT %d\n", __LINE__);
    ttrue(status == 200);
    fprintf(stderr, "AT %d\n", __LINE__);
    response = urlGetResponse(up);
    fprintf(stderr, "AT %d\n", __LINE__);
    ttrue(response != NULL);

    // Verify nonce count incremented
    ttrue(up->nc == 2);

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        tinfo("HTTP=%s, HTTPS=%s", HTTP ? HTTP : "NULL", HTTPS ? HTTPS : "NULL");
        tinfo("Testing Basic authentication - success case");
        testBasicAuthSuccess();

        tinfo("Testing Basic authentication - fail case");
        testBasicAuthFailure();

        tinfo("Testing Digest SHA-256 authentication - success case");
        testDigestSHA256Success();

        tinfo("Testing Digest MD5 authentication - success case");
        testDigestMD5Success();

        tinfo("Testing Digest authentication - fail case");
        testDigestAuthFailure();

        tinfo("Testing authentication auto-detection");
        testAuthAutoDetect();

        tinfo("Testing role-based access control");
        testRoleBasedAccess();

        tinfo("Testing nonce reuse optimization");
        testNonceReuse();
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

#else

int main(void)
{
    tskip("URL_AUTH is not enabled");
    return 0;
}

#endif /* URL_AUTH */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
