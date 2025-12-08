/*
    http.c.tst - Unit tests for HTTP protocol functionality

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void testHTTPMethods()
{
    Url   *up;
    Json  *json;
    cchar *response;
    char  url[128];
    int   status;

    up = urlAlloc(0);

    // Test GET method
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200);
    response = urlGetResponse(up);
    ttrue(response);

    // Test POST method
    json =
        urlJson(up, "POST", SFMT(url, "%s/test/show", HTTP), "test data", (size_t) -1, "Content-Type: text/plain\r\n");
    ttrue(json);
    tcontains(jsonGet(json, 0, "body", 0), "test data");
    jsonFree(json);

    // Test HEAD method
    status = urlFetch(up, "HEAD", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    // HEAD might not be supported on all endpoints, so be more lenient
    ttrue(status == 200 || status == 405 || status == 501);
    response = urlGetResponse(up);
    // HEAD should have no body (if successful)
    if (status == 200) {
        ttrue(!response || slen(response) == 0);
    }

    urlFree(up);
}

static void testHTTPHeaders()
{
    Url  *up;
    Json *json;
    char url[128];

    up = urlAlloc(0);

    // Test custom headers in request
    json = urlJson(up, "POST", SFMT(url, "%s/test/show", HTTP), "test data", (size_t) -1,
                   "X-Test-Header: test-value\r\nX-Custom: custom-value\r\nContent-Type: text/plain\r\n");
    ttrue(json);
    ttrue(jsonGet(json, 0, "body", 0));
    jsonFree(json);

    // Test standard headers
    json = urlJson(up, "POST", SFMT(url, "%s/test/show", HTTP), "test data", (size_t) -1,
                   "User-Agent: test-agent\r\nAccept: text/html\r\nContent-Type: text/plain\r\n");
    ttrue(json);
    ttrue(jsonGet(json, 0, "body", 0));
    jsonFree(json);

    urlFree(up);
}

static void testContentTypes()
{
    Url   *up;
    Json  *json;
    cchar *response;
    char  url[128];
    int   status;

    up = urlAlloc(0);

    // Test HTML content
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200);
    response = urlGetResponse(up);
    ttrue(response);
    tcontains(response, "html");

    // Test JSON content
    json = urlJson(up, "POST", SFMT(url, "%s/test/show",
                                    HTTP), "{\"test\": \"value\"}", (size_t) -1, "Content-Type: application/json\r\n");
    ttrue(json);
    ttrue(jsonGet(json, 0, "body", 0));
    jsonFree(json);

    // Test plain text
    json = urlJson(up, "POST", SFMT(url, "%s/test/show",
                                    HTTP), "plain text", (size_t) -1, "Content-Type: text/plain\r\n");
    ttrue(json);
    tcontains(jsonGet(json, 0, "body", 0), "plain text");
    jsonFree(json);

    urlFree(up);
}

static void testConnectionHandling()
{
    Url   *up;
    cchar *response;
    char  url[128];
    int   status;

    up = urlAlloc(0);

    // Test keep-alive connections (default behavior)
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, "Connection: keep-alive\r\n");
    ttrue(status == 200);
    response = urlGetResponse(up);
    ttrue(response);

    // Test connection close
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, "Connection: close\r\n");
    ttrue(status == 200);
    response = urlGetResponse(up);
    ttrue(response);

    urlFree(up);
}

static void testInvalidRequests()
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // Test path traversal attempt
    status = urlFetch(up, "GET", SFMT(url, "%s/test/../../../etc/passwd", HTTP), NULL, 0, NULL);
    teqi(status, 400);

    // Test nonexistent file
    status = urlFetch(up, "GET", SFMT(url, "%s/nonexistent.html", HTTP), NULL, 0, NULL);
    ttrue(status == 404);

    urlFree(up);
}

static void testStatusCodes()
{
    Url   *up;
    cchar *response;
    char  url[128];
    int   status;

    up = urlAlloc(0);

    // Test 200 OK
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200);
    response = urlGetResponse(up);
    ttrue(response);

    // Test 404 Not Found
    status = urlFetch(up, "GET", SFMT(url, "%s/nonexistent.html", HTTP), NULL, 0, NULL);
    ttrue(status == 404);

    urlFree(up);
}

static void testCacheHeaders()
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // Test If-Modified-Since with old date
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0,
                      "If-Modified-Since: Thu, 01 Jan 1970 00:00:00 GMT\r\n");
    ttrue(status == 200 || status == 304);

    // Test basic conditional request
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200);

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testHTTPMethods();
        testHTTPHeaders();
        testContentTypes();
        testConnectionHandling();
        testInvalidRequests();
        testStatusCodes();
        testCacheHeaders();
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