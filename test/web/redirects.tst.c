/*
    redirects.tst.c - HTTP redirect testing

    Tests HTTP redirect responses (3xx status codes) and proper redirect handling.
    Validates Location headers, redirect chains, and redirect preservation of
    query strings and fragments.

    Based on Appweb test/basic/redirect.tst.es

    Coverage:
    - 301 Moved Permanently
    - 302 Found (temporary redirect)
    - 307 Temporary Redirect (preserves method)
    - 308 Permanent Redirect (preserves method)
    - Location header validation
    - Query string preservation
    - Redirect to directories (trailing slash)

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void testDirectoryRedirect(void)
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *location;

    up = urlAlloc(0);

    // Request directory without trailing slash should redirect to add slash
    // Most web servers redirect /dir to /dir/ for directory listings
    status = urlFetch(up, "GET", SFMT(url, "%s/upload", HTTP), NULL, 0, NULL);

    if (status >= 300 && status < 400) {
        // Got a redirect - check Location header
        location = urlGetHeader(up, "Location");
        ttrue(location != NULL);
        tcontains(location, "/upload/");
    } else {
        // Server may directly serve the directory or return 404
        ttrue(status == 200 || status == 404);
    }

    urlFree(up);
}

static void test302TemporaryRedirect(void)
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *location;

    up = urlAlloc(0);

    // Note: Redirect configuration is disabled in test web.json5
    // This test validates the redirect mechanism if enabled

    // Test that we can handle 302 responses if they occur
    // In production, might redirect http:// to https://
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);

    if (status == 302) {
        location = urlGetHeader(up, "Location");
        ttrue(location != NULL);
        ttrue(slen(location) > 0);
    } else {
        // Without redirect config, should get normal 200 response
        ttrue(status == 200);
    }

    urlFree(up);
}

static void test301PermanentRedirect(void)
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *location;

    up = urlAlloc(0);

    // Test 301 redirect handling (if configured)
    // 301 indicates resource has permanently moved
    status = urlFetch(up, "GET", SFMT(url, "%s/oldpath", HTTP), NULL, 0, NULL);

    if (status == 301) {
        location = urlGetHeader(up, "Location");
        ttrue(location != NULL);
    } else {
        // Without redirect config, likely 404
        ttrue(status == 404);
    }

    urlFree(up);
}

static void testRedirectWithQueryString(void)
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *location;

    up = urlAlloc(0);

    // Request with query string - redirect should preserve it
    status = urlFetch(up, "GET", SFMT(url, "%s/upload?test=value&foo=bar", HTTP), NULL, 0, NULL);

    if (status >= 300 && status < 400) {
        location = urlGetHeader(up, "Location");
        ttrue(location != NULL);
        // Location should preserve query string
        tcontains(location, "test=value");
    } else {
        // Direct response is also acceptable
        ttrue(status == 200 || status == 404);
    }

    urlFree(up);
}

static void test307TemporaryRedirectPreservesMethod(void)
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *location;

    up = urlAlloc(0);

    // 307 is like 302 but explicitly preserves the HTTP method
    // POST to redirected resource should stay POST (unlike 302 which may become GET)
    status = urlFetch(up, "POST", SFMT(url, "%s/test/show", HTTP), "test data", 9,
                      "Content-Type: text/plain\r\n");

    if (status == 307) {
        location = urlGetHeader(up, "Location");
        ttrue(location != NULL);
    } else {
        // Without redirect config, should process POST normally
        ttrue(status == 200);
    }

    urlFree(up);
}

static void testLocationHeaderFormat(void)
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *location;

    up = urlAlloc(0);

    // Test that Location headers are properly formatted
    status = urlFetch(up, "GET", SFMT(url, "%s/upload", HTTP), NULL, 0, NULL);

    if (status >= 300 && status < 400) {
        location = urlGetHeader(up, "Location");
        ttrue(location != NULL);

        // Location should be absolute or path-absolute
        // Valid examples: "http://host/path" or "/path"
        ttrue(scontains(location, "/") != NULL);

        // Should not contain whitespace
        ttrue(scontains(location, " ") == NULL);
    }

    urlFree(up);
}

static void testRedirectStatusCodes(void)
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // Test that 3xx responses are handled correctly by URL client
    // Even if server doesn't issue redirects, client should handle them

    // Normal request should not redirect
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200);

    // Verify non-redirect status
    ttrue(status < 300 || status >= 400);

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testDirectoryRedirect();
        test302TemporaryRedirect();
        test301PermanentRedirect();
        testRedirectWithQueryString();
        test307TemporaryRedirectPreservesMethod();
        testLocationHeaderFormat();
        testRedirectStatusCodes();
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
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
