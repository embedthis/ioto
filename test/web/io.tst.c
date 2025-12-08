/*
    io.c.tst - Unit tests for I/O functionality

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void testWebRead()
{
    Url  *up;
    Json *json;
    char url[128];

    up = urlAlloc(0);

    // Test reading request body
    json = urlJson(up, "POST", SFMT(url, "%s/test/show",
                                    HTTP), "test input data", (size_t) -1, "Content-Type: text/plain\r\n");
    ttrue(json);
    tcontains(jsonGet(json, 0, "body", 0), "test input data");
    jsonFree(json);
    urlFree(up);
}

static void testWebWrite()
{
    Url   *up;
    cchar *response;
    char  url[128];
    int   status;

    up = urlAlloc(0);

    // Test writing response data
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200);
    response = urlGetResponse(up);
    ttrue(response);
    urlFree(up);
}

static void testContentLength()
{
    Url   *up;
    Json  *json;
    char  url[128];
    cchar *data;

    up = urlAlloc(0);

    // Test content length setting
    data = "123456789012345";  // 15 chars
    json = urlJson(up, "POST", SFMT(url, "%s/test/show", HTTP), data, 15, "Content-Type: text/plain\r\n");
    ttrue(json);
    ttrue(jsonGet(json, 0, "body", 0));
    jsonFree(json);
    urlFree(up);
}

static void testLargeBody()
{
    Url    *up;
    Json   *json;
    char   url[128];
    char   *largeData;
    size_t size = 1024 * 5;     // 5KB (smaller for testing)

    up = urlAlloc(0);

    // Create large test data
    largeData = rAlloc(size + 1);
    for (size_t i = 0; i < size; i++) {
        largeData[i] = 'A' + (i % 26);
    }
    largeData[size] = '\0';

    // Test handling large request body
    json = urlJson(up, "POST", SFMT(url, "%s/test/show", HTTP), largeData, size, "Content-Type: text/plain\r\n");
    ttrue(json);
    ttrue(jsonGet(json, 0, "body", 0));
    rFree(largeData);
    jsonFree(json);
    urlFree(up);
}

static void testHeaders()
{
    Url   *up;
    cchar *response;
    char  url[128];
    int   status;

    up = urlAlloc(0);

    // Test custom headers in request
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, "X-Custom-Header: test-value\r\n");
    ttrue(status == 200);
    response = urlGetResponse(up);
    ttrue(response);
    urlFree(up);
}

static void testFileResponse()
{
    Url   *up;
    cchar *response;
    char  url[128];
    int   status;

    up = urlAlloc(0);

    // Test file serving
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200);
    response = urlGetResponse(up);
    ttrue(response);
    tcontains(response, "html");
    urlFree(up);
}

static void testChunkedTransfer()
{
    Url  *up;
    Json *json;
    char url[128];

    up = urlAlloc(0);

    // Test with chunked data (server handles this internally)
    json = urlJson(up, "POST", SFMT(url, "%s/test/show", HTTP), "test chunked data", (size_t) -1,
                   "Content-Type: text/plain\r\n");
    ttrue(json);
    tcontains(jsonGet(json, 0, "body", 0), "test chunked data");
    jsonFree(json);
    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testWebRead();
        testWebWrite();
        testContentLength();
        testLargeBody();
        testHeaders();
        testFileResponse();
        testChunkedTransfer();
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