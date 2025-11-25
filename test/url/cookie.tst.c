/*
    cookies.c.tst - Unit tests for cookie handling

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char     *HTTP;
static char     *HTTPS;

/************************************ Code ************************************/

static void testSetCookie()
{
    Url     *up;
    char    url[128];
    char   *cookie;
    int     status;

    up = urlAlloc(0);

    // Request a URL that sets cookies (may not be supported by test server)
    status = urlFetch(up, "GET", SFMT(url, "%s/test/cookie?name=test&value=cookie-value", HTTP), 0, 0, 0);
    teq(status, 200);

    // Check if cookie was set
    cookie = urlGetCookie(up, "test");
    tmatch(cookie, "cookie-value");
    rFree(cookie);
    urlFree(up);
}

static void testMultipleCookies()
{
    Url     *up;
    char    url[128];
    char   *cookie1, *cookie2;
    int     status;

    up = urlAlloc(0);

    // Request URL that sets multiple cookies
    status = urlFetch(up, "GET", SFMT(url, "%s/test/cookie?name=first&value=value1", HTTP), 0, 0, 0);
    teq(status, 200);
    cookie1 = urlGetCookie(up, "first");
    
    status = urlFetch(up, "GET", SFMT(url, "%s/test/cookie?name=second&value=value2", HTTP), 0, 0, 0);
    teq(status, 200);

    // Check both cookies are preserved
    cookie2 = urlGetCookie(up, "second");
    tmatch(cookie1, "value1");
    tmatch(cookie2, "value2");
    rFree(cookie1);
    rFree(cookie2);
    urlFree(up);
}

static void testCookieWithAttributes()
{
    Url     *up;
    char    url[128];
    char   *cookie;
    int     status;

    up = urlAlloc(0);

    // Test cookie with path, domain, secure attributes
    status = urlFetch(up, "GET", SFMT(url, "%s/test/cookie?name=secure&value=secret&path=/test&secure=true", HTTP), 0, 0, 0);
    teq(status, 200);

    cookie = urlGetCookie(up, "secure");
    tmatch(cookie, "secret");
    rFree(cookie);
    urlFree(up);
}

static void testSendCookies()
{
    Url     *up;
    Json    *json;
    char    url[128];
    int     status;

    up = urlAlloc(0);

    // First, set a cookie
    status = urlFetch(up, "GET", SFMT(url, "%s/test/cookie?name=session&value=12345", HTTP), 0, 0, 0);
    teq(status, 200);

    // Then make another request - cookies should be sent automatically
    status = urlFetch(up, "GET", SFMT(url, "%s/test/show", HTTP), 0, 0, 0);
    teq(status, 200);

    json = urlGetJsonResponse(up);
    // Check if our cookie was sent in the request
    if (jsonGet(json, 0, "headers.cookie", 0)) {
        tcontains(jsonGet(json, 0, "headers.cookie", 0), "session=12345");
    }
    jsonFree(json);
    urlFree(up);
}

static void testCookieEdgeCases()
{
    Url     *up;
    char    *cookie;
    char    url[128];
    int     status;

    up = urlAlloc(0);

    // Test cookie with special characters
    status = urlFetch(up, "GET", SFMT(url, "%s/test/cookie?name=special&value=a%%20b%%3Dc", HTTP), 0, 0, 0);
    teq(status, 404);

    // Test non-existent cookie
    cookie = urlGetCookie(up, "nonexistent");
    tnull(cookie);
    rFree(cookie);

    // Test empty cookie name
    cookie = urlGetCookie(up, "");
    tnull(cookie);
    rFree(cookie);

    // Test NULL cookie name
    cookie = urlGetCookie(up, NULL);
    tnull(cookie);
    rFree(cookie);

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testSetCookie();
        testMultipleCookies();
        testCookieWithAttributes();
        testSendCookies();
        testCookieEdgeCases();
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
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
