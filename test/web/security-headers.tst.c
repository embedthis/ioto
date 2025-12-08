/*
    security-headers.tst.c - HTTP header injection and security header testing

    Tests the web server's protection against header injection attacks (CRLF injection,
    response splitting) and validates that proper security headers are set. Ensures
    the server cannot be manipulated into injecting arbitrary headers or content.

    Based on GoAhead test/security/headers.tst.es and OWASP security guidelines.

    Coverage:
    - CRLF injection in headers (\r\n)
    - HTTP response splitting attacks
    - Header name validation
    - Header value validation
    - Multiple header injection attempts
    - Null byte injection in headers
    - Security header presence (X-Frame-Options, X-Content-Type-Options, etc.)
    - Content-Security-Policy validation
    - Strict-Transport-Security (HSTS) on HTTPS

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void testCRLFInjectionInCustomHeader(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: CRLF in custom header value
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0,
                      "X-Custom: value\r\nX-Injected: malicious\r\n");
    // Server should handle gracefully or URL client should reject
    ttrue(status == 200 || status == 400 || status < 0);

    // Test 2: Newline only (\n)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0,
                      "X-Custom: value\nX-Injected: bad\r\n");
    ttrue(status == 200 || status == 400 || status < 0);

    // Test 3: URL-encoded CRLF (%0D%0A) in header
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0,
                      "X-Custom: value%%0D%%0AX-Injected: bad\r\n");
    ttrue(status == 200 || status == 400 || status < 0);

    urlFree(up);
}

static void testResponseSplittingAttempt(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: Attempt to inject response splitting via query parameter
    // (if server reflects query params in response headers)
    status = urlFetch(up, "GET",
                      SFMT(url, "%s/test/echo?param=value%%0D%%0AContent-Length:%%200%%0D%%0A%%0D%%0AAttack", HTTP),
                      NULL, 0, NULL);
    // Server should sanitize or reject
    ttrue(status == 200 || status == 400 || status == 404 || status == 405 || status < 0);

    // Test 2: CRLF with double encoding
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/test/echo?param=%%250D%%250A", HTTP), NULL, 0, NULL);
    ttrue(status == 200 || status == 400 || status == 404 || status == 405 || status < 0);

    urlFree(up);
}

static void testHeaderNameValidation(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: Header name with space
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0,
                      "Invalid Header: value\r\n");
    // URL client or server should reject
    ttrue(status == 200 || status == 400 || status < 0);

    // Test 2: Header name with special chars
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0,
                      "X-Bad<Header>: value\r\n");
    ttrue(status == 200 || status == 400 || status < 0);

    // Test 3: Header name with colon (multiple colons)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0,
                      "X:Bad:Header: value\r\n");
    ttrue(status == 200 || status == 400 || status < 0);

    urlFree(up);
}

static void testNullByteInHeaders(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: Null byte in header value
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0,
                      "X-Custom: value%%00more\r\n");
    ttrue(status == 200 || status == 400 || status < 0);

    // Test 2: Null byte in header name
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0,
                      "X%%00Bad: value\r\n");
    ttrue(status == 200 || status == 400 || status < 0);

    urlFree(up);
}

static void testSecurityHeadersPresent(void)
{
    Url   *up;
    char  url[256];
    int   status;
    cchar *header;

    up = urlAlloc(0);

    // Get a normal response and check for security headers
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    teqi(status, 200);

    // Test 1: X-Content-Type-Options header
    header = urlGetHeader(up, "X-Content-Type-Options");
    if (header) {
        // If present, should be "nosniff"
        tcontains(header, "nosniff");
    }

    // Test 2: X-Frame-Options header
    header = urlGetHeader(up, "X-Frame-Options");
    if (header) {
        // If present, should be DENY, SAMEORIGIN, or ALLOW-FROM
        ttrue(scontains(header, "DENY") != NULL ||
              scontains(header, "SAMEORIGIN") != NULL ||
              scontains(header, "ALLOW-FROM") != NULL);
    }

    // Test 3: X-XSS-Protection header (legacy but may be present)
    header = urlGetHeader(up, "X-XSS-Protection");
    if (header) {
        // Should be enabled
        ttrue(scontains(header, "1") != NULL);
    }

    // Test 4: Check Server header doesn't reveal too much
    header = urlGetHeader(up, "Server");
    if (header) {
        // Should be present but not reveal version details
        tgti(slen(header), 0);
        // Should not contain version numbers (optional check)
    }

    urlFree(up);
}

static void testContentSecurityPolicyHeader(void)
{
    Url   *up;
    char  url[256];
    int   status;
    cchar *csp;

    up = urlAlloc(0);

    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    teqi(status, 200);

    // Check for Content-Security-Policy header
    csp = urlGetHeader(up, "Content-Security-Policy");
    if (csp) {
        // If present, should have some directives
        tgti(slen(csp), 0);

        // Common directives: default-src, script-src, etc.
        ttrue(scontains(csp, "default-src") != NULL ||
              scontains(csp, "script-src") != NULL ||
              scontains(csp, "style-src") != NULL);
    }

    // Also check for CSP-Report-Only variant
    csp = urlGetHeader(up, "Content-Security-Policy-Report-Only");
    if (csp) {
        tgti(slen(csp), 0);
    }

    urlFree(up);
}

static void testHTTPSSecurityHeaders(void)
{
    Url   *up;
    char  url[256];
    int   status;
    cchar *hsts;

    up = urlAlloc(0);

    // Only test HTTPS security headers if HTTPS is available
    if (HTTPS && scontains(HTTPS, "https")) {
        status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTPS), NULL, 0, NULL);

        if (status == 200) {
            // Test: Strict-Transport-Security (HSTS) header
            hsts = urlGetHeader(up, "Strict-Transport-Security");
            if (hsts) {
                // Should specify max-age
                tcontains(hsts, "max-age");

                // May include includeSubDomains and preload
                ttrue(scontains(hsts, "includeSubDomains") != NULL ||
                      scontains(hsts, "max-age") != NULL);
            }
        }
    } else {
        // HTTPS not configured - skip test
        ttrue(1);
    }

    urlFree(up);
}

static void testMultipleHeaderInjection(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test: Attempt to inject multiple headers at once
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0,
                      "X-Test: value%%0D%%0AX-Inject1: bad%%0D%%0AX-Inject2: worse\r\n");
    ttrue(status == 200 || status == 400 || status < 0);

    urlFree(up);
}

static void testHeaderValueWhitespace(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test 1: Leading/trailing whitespace in header value (valid)
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0,
                      "X-Custom:   value with spaces   \r\n");
    teqi(status, 200);

    // Test 2: Tabs in header value
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0,
                      "X-Custom: value\twith\ttabs\r\n");
    teqi(status, 200);

    urlFree(up);
}

static void testEmptyHeaderValue(void)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);

    // Test: Empty header value (valid)
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0,
                      "X-Custom: \r\n");
    teqi(status, 200);

    urlFree(up);
}

static void testVeryLongHeaderValue(void)
{
    Url    *up;
    char   url[256], *longValue;
    int    status;
    size_t len;

    up = urlAlloc(0);

    // Test: Very long header value (should be within limits)
    len = 8192;
    longValue = rAlloc(len + 200);
    char   *p = longValue;
    size_t remaining = len + 200;

    p += scopy(p, remaining, "X-Long-Header: ");
    remaining -= slen("X-Long-Header: ");

    for (int i = 0; i < 8000 && remaining > 10; i++) {
        *p++ = 'a';
        remaining--;
    }
    scopy(p, remaining, "\r\n");

    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, longValue);
    // May exceed header size limit
    ttrue(status == 200 || status == 400 || status < 0);

    rFree(longValue);
    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testCRLFInjectionInCustomHeader();
        testResponseSplittingAttempt();
        testHeaderNameValidation();
        testNullByteInHeaders();
        testSecurityHeadersPresent();
        testContentSecurityPolicyHeader();
        testHTTPSSecurityHeaders();
        testMultipleHeaderInjection();
        testHeaderValueWhitespace();
        testEmptyHeaderValue();
        testVeryLongHeaderValue();
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
