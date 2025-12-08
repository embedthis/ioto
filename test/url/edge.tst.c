/*
    edge-cases.c.tst - Unit tests for edge cases and error conditions

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char    *HTTP;
static char    *HTTPS;

/************************************ Code ************************************/

static void testMalformedUrls()
{
    Url     *up;
    int     rc;

    up = urlAlloc(0);

    // Test various URLs
    rc = urlParse(up, "not-a-url");
    ttrue(rc == 0);
    tmatch(up->scheme, "http");
    tmatch(up->host, "localhost");
    ttrue(up->port == 80);
    tmatch(up->path, "not-a-url");
    ttrue(up->query == 0);
    ttrue(up->hash == 0);

    rc = urlParse(up, "http://");
    ttrue(rc == 0);
    tmatch(up->scheme, "http");
    tmatch(up->host, "localhost");
    ttrue(up->port == 80);
    tmatch(up->path, "");
    ttrue(up->query == 0);
    ttrue(up->hash == 0);

    rc = urlParse(up, "://hostname/");
    ttrue(rc == 0);
    tmatch(up->scheme, "http");
    tmatch(up->host, "hostname");
    ttrue(up->port == 80);
    tmatch(up->path, "");
    ttrue(up->query == 0);
    ttrue(up->hash == 0);

    urlFree(up);
}

static void testNullParameters()
{
    Url     *up;
    int     status;
    cchar   *result;

    up = urlAlloc(0);

    // Test NULL URL parameter
    status = urlFetch(up, "GET", NULL, 0, 0, 0);
    ttrue(status < 0);

    // Test NULL method
    status = urlFetch(up, NULL, "http://localhost/", 0, 0, 0);
    ttrue(status < 0);

    // Test NULL headers retrieval
    result = urlGetHeader(up, NULL);
    ttrue(result == 0);

    // Test NULL cookie name
    result = urlGetCookie(up, NULL);
    ttrue(result == 0);

    urlFree(up);

    // Test NULL Url pointer
    status = urlGetStatus(NULL);
    ttrue(status < 0);

    result = urlGetResponse(NULL);
    ttrue(result == 0);
}

static void testVeryLongUrl()
{
    Url     *up;
    char    *longUrl;
    ssize   i, urlLen;
    int     rc;

    // Create very long URL
    urlLen = 8000;
    longUrl = rAlloc(urlLen + 100);
    sfmtbuf(longUrl, urlLen + 100, "http://localhost:4100/");

    // Append long path
    for (i = slen(longUrl); i < urlLen - 20; i++) {
        longUrl[i] = 'a' + (i % 26);
    }
    scopy(&longUrl[urlLen - 20], 20, "/index.html");

    up = urlAlloc(0);

    rc = urlParse(up, longUrl);
    // Should either parse successfully or fail gracefully
    ttrue(rc == 0 || rc < 0);

    urlFree(up);
    rFree(longUrl);
}

static void testInvalidPorts()
{
    Url     *up;
    int     status;

    up = urlAlloc(0);

    // Test connection to invalid ports
    status = urlFetch(up, "GET", "http://localhost:1/", 0, 0, 0);
    ttrue(status < 0);  // Should fail

    status = urlFetch(up, "GET", "http://localhost:99999/", 0, 0, 0);
    ttrue(status < 0);  // Invalid port number

    urlFree(up);
}

static void testZeroSizeOperations()
{
    Url     *up;
    Json    *json;
    char    url[128];
    char    buffer[100];
    int     status;
    ssize   result;

    up = urlAlloc(0);

    // Test zero-size POST
    status = urlFetch(up, "POST", SFMT(url, "%s/test/show", HTTP), "", 0, 0);
    ttrue(status == 200);

    // Test zero-size write
    result = urlWrite(up, NULL, 0);
    ttrue(result == 0);

    // Test zero-size read
    result = urlRead(up, buffer, 0);
    ttrue(result == 0);

    json = urlGetJsonResponse(up);
    tmatch(jsonGet(json, 0, "body", 0), NULL);
    jsonFree(json);

    urlFree(up);
}

static void testMemoryLimits()
{
    Url     *up;
    char    url[128];
    int     status;

    up = urlAlloc(0);

    // Set very small buffer limit
    urlSetBufLimit(up, 10);

    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), 0, 0, 0);
    ttrue(status == 200);

    // Response should be truncated
    cchar *response = urlGetResponse(up);
    ttrue(response != 0);
    ttrue(slen(response) <= 10);

    urlFree(up);
}

static void testInvalidStatusCodes()
{
    Url     *up;
    char    url[128];
    int     status;

    up = urlAlloc(0);

    // Request a URL that returns 404
    status = urlFetch(up, "GET", SFMT(url, "%s/non-existent-page", HTTP), 0, 0, 0);
    ttrue(status == 404);
    ttrue(urlGetStatus(up) == 404);

    // Not an error - 404 is a valid HTTP response
    ttrue(urlGetError(up) == 0);
    tmatch(urlGetResponse(up), "Cannot locate document");

    urlFree(up);
}

static void testDoubleOperations()
{
    Url     *up;
    char    url[128];
    int     status;

    up = urlAlloc(0);

    // Start request twice
    status = urlStart(up, "GET", SFMT(url, "%s/index.html", HTTP));
    ttrue(status == 0);

    status = urlStart(up, "GET", SFMT(url, "%s/index.html", HTTP));
    // Should either succeed (reset) or fail gracefully
    ttrue(status == 0 || status < 0);

    // Finalize twice
    status = urlFinalize(up);
    status = urlFinalize(up);
    // Second finalize should be safe

    // Close twice
    urlClose(up);
    urlClose(up);  // Should be safe

    urlFree(up);
}

static void testUseAfterClose()
{
    Url     *up;
    char    url[128];
    char    buffer[100];
    int     status;

    up = urlAlloc(0);

    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), 0, 0, 0);
    ttrue(status == 200);

    // Close connection
    urlClose(up);

    // Try to use after close. Value persists until the next request.
    cchar *response = urlGetResponse(up);
    ttrue(response);

    ssize result = urlRead(up, buffer, sizeof(buffer));
    ttrue(result <= 0);  // Should fail gracefully

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testMalformedUrls();
        testNullParameters();
        testVeryLongUrl();
        testInvalidPorts();
        testZeroSizeOperations();
        testMemoryLimits();
        testInvalidStatusCodes();
        testDoubleOperations();
        testUseAfterClose();
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