/*
    api.c.tst - Unit tests for core API functionality

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char    *HTTP;
static char    *HTTPS;

/************************************ Code ************************************/

static void testBasicAllocation()
{
    Url     *up;

    // Test basic allocation and freeing
    up = urlAlloc(0);
    ttrue(up != 0);

    // Test initial state
    ttrue(urlGetStatus(up) < 0);
    ttrue(urlGetResponse(up));
    ttrue(urlGetResponse(up)[0] == '\0');

    urlFree(up);
    ttrue(1);  // Should not crash
}

static void testParseValidUrls()
{
    Url     *up;
    int     status;

    up = urlAlloc(0);

    // Test basic HTTP URL parsing
    status = urlParse(up, "http://www.example.com/path?query=value#hash");
    ttrue(status == 0);
    tmatch(up->scheme, "http");
    tmatch(up->host, "www.example.com");
    ttrue(up->port == 80);
    tmatch(up->path, "path");
    tmatch(up->query, "query=value");
    tmatch(up->hash, "hash");

    // Test HTTPS URL parsing
    status = urlParse(up, "https://secure.com:8443/");
    ttrue(status == 0);
    tmatch(up->scheme, "https");
    tmatch(up->host, "secure.com");
    ttrue(up->port == 8443);

    urlFree(up);
}

static void testParseInvalidUrls()
{
    Url     *up;
    int     rc;

    up = urlAlloc(0);

    // Test malformed URLs
    rc = urlParse(up, NULL);
    ttrue(rc < 0);

    rc = urlParse(up, "");
    ttrue(rc == 0);  // Should use defaults
    tmatch(up->scheme, "http");
    tmatch(up->host, "localhost");
    ttrue(up->port == 80);
    tmatch(up->path, "");
    ttrue(up->query == 0);
    ttrue(up->hash == 0);

    rc = urlParse(up, "not-a-url");
    ttrue(rc == 0);  // Interpret as just the path portion
    tmatch(up->scheme, "http");
    tmatch(up->host, "localhost");
    ttrue(up->port == 80);
    tmatch(up->path, "not-a-url");
    ttrue(up->query == 0);
    ttrue(up->hash == 0);

    rc = urlParse(up, "http://");
    ttrue(rc == 0);  // localhost and port 80
    tmatch(up-> scheme, "http");
    tmatch(up->host, "localhost");
    ttrue(up->port == 80);
    tmatch(up->path, "");
    ttrue(up->query == 0);
    ttrue(up->hash == 0);

    urlFree(up);
}

static void testConfiguration()
{
    Url     *up;

    up = urlAlloc(0);

    // Test timeout configuration
    urlSetTimeout(up, 30 * TPS);
    ttrue(1);  // Should not crash

    // Test buffer limit
    urlSetBufLimit(up, 10000);
    ttrue(1);  // Should not crash

    // Test flags
    urlSetFlags(up, URL_SHOW_REQ_HEADERS);
    ttrue(1);  // Should not crash
    urlSetFlags(up, 0);

    // Test status setting
    // Cannot use urlGetStatus here as it will finalize the request
    urlSetStatus(up, 200);
    ttrue(up->status == 200);

    // Test protocol setting
    urlSetProtocol(up, 0);  // HTTP/1.0
    urlSetProtocol(up, 1);  // HTTP/1.1
    ttrue(1);  // Should not crash

    urlFree(up);
}

static void testSimpleGet()
{
    Url     *up;
    char    url[128];
    cchar   *response;
    int     status;

    up = urlAlloc(0);

    // Test simple GET request
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), 0, 0, 0);
    ttrue(status == 200);
    ttrue(urlGetStatus(up) == 200);

    response = urlGetResponse(up);
    ttrue(response != 0);
    ttrue(slen(response) > 0);

    // Test headers
    cchar *contentType = urlGetHeader(up, "Content-Type");
    ttrue(contentType != 0);

    cchar *contentLength = urlGetHeader(up, "Content-Length");
    ttrue(contentLength != 0);

    urlFree(up);
}

static void testSimplePost()
{
    Url     *up;
    char    url[128];
    char    *data = "test=value";
    int     status;

    up = urlAlloc(0);

    // Test simple POST request
    status = urlFetch(up, "POST", SFMT(url, "%s/test/show", HTTP), data, 0, 0);
    ttrue(status == 200);

    cchar *response = urlGetResponse(up);
    ttrue(response != 0);

    urlFree(up);
}

static void testNullSafety()
{
    // Test NULL parameter handling
    ttrue(urlGetStatus(NULL) < 0);
    ttrue(urlGetResponse(NULL) == 0);
    ttrue(urlGetError(NULL) == 0);
    ttrue(urlGetHeader(NULL, "test") == 0);
    ttrue(urlGetCookie(NULL, "test") == 0);

    Url *up = urlAlloc(0);

    // Test NULL parameters with valid object
    ttrue(urlGetHeader(up, NULL) == 0);
    ttrue(urlGetCookie(up, NULL) == 0);
    ttrue(urlFetch(up, NULL, "http://test", 0, 0, 0) < 0);
    ttrue(urlFetch(up, "GET", NULL, 0, 0, 0) < 0);

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testBasicAllocation();
        testParseValidUrls();
        testParseInvalidUrls();
        testConfiguration();
        testSimpleGet();
        testSimplePost();
        testNullSafety();
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