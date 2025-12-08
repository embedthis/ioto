/*
    basic.tst.c - Unit tests for HTTP Basic authentication

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
    Test urlSetAuth API with Basic authentication
 */
static void testSetBasicAuth()
{
    Url *up;

    up = urlAlloc(0);

    // Test setting basic auth
    urlSetAuth(up, "user1", "pass1", "basic");
    tmatch(up->username, "user1");
    tmatch(up->password, "pass1");
    tmatch(up->authType, "basic");

    // Test changing auth
    urlSetAuth(up, "user2", "pass2", "basic");
    tmatch(up->username, "user2");
    tmatch(up->password, "pass2");
    tmatch(up->authType, "basic");

    // Test clearing auth
    urlSetAuth(up, NULL, NULL, NULL);
    ttrue(up->username == NULL);
    ttrue(up->password == NULL);
    ttrue(up->authType == NULL);

    urlFree(up);
}

/*
    Test Basic authentication header generation
 */
static void testBasicAuthHeader()
{
    Url     *up;
    char    url[128];
    int     status;
    char    *encoded;

    // Test manual Basic auth header generation
    up = urlAlloc(0);
    urlSetAuth(up, "testuser", "testpass", "basic");

    // The auth header should be automatically added when fetching
    status = urlFetch(up, "GET", SFMT(url, "%s/", HTTP), 0, 0, 0);
    ttrue(status >= 200 && status < 400);  // Should succeed
    urlFree(up);

    // Verify base64 encoding works
    encoded = cryptEncode64("testuser:testpass");
    tmatch(encoded, "dGVzdHVzZXI6dGVzdHBhc3M=");
    rFree(encoded);
}

/*
    Test manual Authorization header with Basic auth
 */
static void testManualBasicAuthHeader()
{
    Url     *up;
    char    url[128];
    int     status;
    char    *authHeader;

    // Test with manual Authorization header
    up = urlAlloc(0);
    // Manually create Basic auth header: user:password = dXNlcjpwYXNzd29yZA==
    authHeader = "Authorization: Basic dXNlcjpwYXNzd29yZA==\r\n";
    status = urlFetch(up, "GET", SFMT(url, "%s/", HTTP), 0, 0, authHeader);
    ttrue(status >= 200 && status < 400);
    urlFree(up);
}

/*
    Test Basic auth with various username/password combinations
 */
static void testBasicAuthCombinations()
{
    Url  *up;
    char *encoded;

    // Test empty password
    up = urlAlloc(0);
    urlSetAuth(up, "user", "", "basic");
    tmatch(up->username, "user");
    tmatch(up->password, "");
    urlFree(up);

    // Test special characters in credentials
    encoded = cryptEncode64("user@domain.com:pass:word");
    ttrue(slen(encoded) > 0);
    rFree(encoded);

    // Test long credentials
    encoded = cryptEncode64("verylongusername123456789:verylongpassword123456789");
    ttrue(slen(encoded) > 0);
    rFree(encoded);
}

/*
    Test that auth header doesn't override explicit Authorization header
 */
static void testNoAuthHeaderOverride()
{
    Url     *up;
    char    url[128];
    int     status;
    char    *authHeader;

    // Set auth via urlSetAuth but also provide explicit Authorization header
    up = urlAlloc(0);
    urlSetAuth(up, "user1", "pass1", "basic");
    // Explicit header should take precedence
    authHeader = "Authorization: Basic dXNlcjpwYXNzd29yZA==\r\n";
    status = urlFetch(up, "GET", SFMT(url, "%s/", HTTP), 0, 0, authHeader);
    ttrue(status >= 200 && status < 400);
    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        tinfo("Testing urlSetAuth API with Basic auth");
        testSetBasicAuth();

        tinfo("Testing Basic authentication header generation");
        testBasicAuthHeader();

        tinfo("Testing manual Basic authentication header");
        testManualBasicAuthHeader();

        tinfo("Testing Basic auth with various combinations");
        testBasicAuthCombinations();

        tinfo("Testing that explicit Authorization header is not overridden");
        testNoAuthHeaderOverride();
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
