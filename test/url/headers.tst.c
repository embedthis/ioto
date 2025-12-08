/*
    headers.c.tst - Unit tests for HTTP header handling

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char    *HTTP;
static char    *HTTPS;

/************************************ Code ************************************/

static void testRequestHeaders()
{
    Url     *up;
    Json    *json;
    char    url[128];
    int     status;

    up = urlAlloc(0);

    // Test custom request headers
    status = urlFetch(up, "GET", SFMT(url, "%s/test/show", HTTP), 0, 0,
                     "X-Custom-Header: test-value\r\n"
                     "User-Agent: url-test-client/1.0\r\n");
    ttrue(status == 200);

    /*
        Although the headers are case insensitive, the JSON keys from test() in the web server
        are case sensitive and preserve the original case.
     */
    json = urlGetJsonResponse(up);
    tmatch(jsonGet(json, 0, "headers.X-Custom-Header", 0), "test-value");
    tmatch(jsonGet(json, 0, "headers.User-Agent", 0), "url-test-client/1.0");
    jsonFree(json);

    urlFree(up);
}

static void testResponseHeaders()
{
    Url     *up;
    cchar   *header;
    char    url[128];
    int     status;

    up = urlAlloc(0);

    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), 0, 0, 0);
    ttrue(status == 200);

    // Test common response headers
    header = urlGetHeader(up, "Content-Type");
    ttrue(header != 0);
    ttrue(scontains(header, "text/html"));

    header = urlGetHeader(up, "Content-Length");
    ttrue(header != 0);
    ttrue(stoi(header) > 0);

    // The web server does not set a Server header for security -- less information leakage
    header = urlGetHeader(up, "Server");
    ttrue(header == 0);

    // Test case insensitive header lookup
    header = urlGetHeader(up, "content-type");
    ttrue(header != 0);
    ttrue(scontains(header, "text/html"));

    // Test non-existent header
    header = urlGetHeader(up, "X-Non-Existent");
    ttrue(header == 0);

    urlFree(up);
}

static void testHeaderEdgeCases()
{
    Url     *up;
    Json    *json;
    char    url[128];
    int     status;

    up = urlAlloc(0);

    // Test headers with special characters and spaces
    status = urlFetch(up, "GET", SFMT(url, "%s/test/show", HTTP), 0, 0,
                     "X-Special-Chars: value with spaces and symbols !@#$\r\n"
                     "X-Unicode: Héllo Wörld 测试\r\n"
                     "X-Empty:\r\n");
    ttrue(status == 200);

    json = urlGetJsonResponse(up);
    tmatch(jsonGet(json, 0, "headers.X-Special-Chars", 0), "value with spaces and symbols !@#$");
    tmatch(jsonGet(json, 0, "headers.X-Unicode", 0), "Héllo Wörld 测试");
    tmatch(jsonGet(json, 0, "headers.X-Empty", 0), "");
    jsonFree(json);

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testRequestHeaders();
        testResponseHeaders();
        testHeaderEdgeCases();
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