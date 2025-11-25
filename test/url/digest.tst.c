/*
    digest.tst.c - Unit tests for HTTP Digest authentication

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
    Test urlSetAuth API with Digest authentication
 */
static void testSetDigestAuth()
{
    Url *up;

    up = urlAlloc(0);

    // Test setting digest auth
    urlSetAuth(up, "digestuser", "digestpass", "digest");
    tmatch(up->username, "digestuser");
    tmatch(up->password, "digestpass");
    tmatch(up->authType, "digest");

    // Initially no challenge info
    ttrue(up->realm == NULL);
    ttrue(up->nonce == NULL);
    ttrue(up->qop == NULL);
    ttrue(up->nc == 0);

    urlFree(up);
}

/*
    Test Digest auth state after configuration
 */
static void testDigestAuthState()
{
    Url *up;

    up = urlAlloc(0);
    urlSetAuth(up, "digestuser", "digestpass", "digest");

    // Verify state is set correctly
    tmatch(up->username, "digestuser");
    tmatch(up->password, "digestpass");
    tmatch(up->authType, "digest");

    // Initially no challenge info until server responds with 401
    ttrue(up->realm == NULL);
    ttrue(up->nonce == NULL);
    ttrue(up->qop == NULL);
    ttrue(up->nc == 0);
    urlFree(up);
}

/*
    Test auto-detect authentication type (NULL authType)
 */
static void testAuthAutoDetect()
{
    Url *up;

    up = urlAlloc(0);
    urlSetAuth(up, "user", "password", NULL);  // Auto-detect

    tmatch(up->username, "user");
    tmatch(up->password, "password");
    ttrue(up->authType == NULL);  // Will be set after server challenge

    urlFree(up);
}

/*
    Test MD5 digest calculation
 */
static void testDigestMD5()
{
    char *hash;

    // Test known MD5 hash
    hash = cryptGetMd5((uchar *) "test", 4);
    tmatch(hash, "098f6bcd4621d373cade4e832627b4f6");
    rFree(hash);

    // Test MD5 of empty string
    hash = cryptGetMd5((uchar *) "", 0);
    tmatch(hash, "d41d8cd98f00b204e9800998ecf8427e");
    rFree(hash);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        tinfo("Testing urlSetAuth API with Digest auth");
        testSetDigestAuth();

        tinfo("Testing Digest auth state");
        testDigestAuthState();

        tinfo("Testing auth type auto-detection");
        testAuthAutoDetect();

        tinfo("Testing MD5 digest calculation");
        testDigestMD5();
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
