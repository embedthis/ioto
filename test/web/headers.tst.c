

/*
    headers.c.tst - Unit tests for response headers

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void checkResponseHeaders()
{
    Url   *up;
    cchar *response;
    char  url[128];
    int   status;

    up = urlAlloc(0);

    status = urlFetch(up, "GET", SFMT(url, "%s/test/success", HTTP), NULL, 0, NULL);
    teqi(status, 200);
    response = urlGetResponse(up);
    tmatch(response, "success\n");

    //  Check expected headers
    tmatch(urlGetHeader(up, "Content-Type"), "text/plain");
    tmatch(urlGetHeader(up, "Content-Length"), "8");
    tmatch(urlGetHeader(up, "Connection"), "keep-alive");

    //  Check case insensitive
    tmatch(urlGetHeader(up, "connection"), "keep-alive");
    tmatch(urlGetHeader(up, "content-length"), "8");

    //  Check unexpected headers
    tnull(urlGetHeader(up, "Last-Modified"));
    tnull(urlGetHeader(up, "ETag"));


    //  Static file
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    teqi(status, 200);
    //  Check expected headers
    tnotnull(urlGetHeader(up, "Last-Modified"));
    tnotnull(urlGetHeader(up, "ETag"));

    urlFree(up);
}

static void setHeaders()
{
    Json *json;
    char url[128];

    json = urlGetJson(SFMT(url, "%s/test/show", HTTP), "X-TEST: 42\r\n");
    tmatch(jsonGet(json, 0, "headers['X-TEST']", 0), "42");
    jsonFree(json);
}

static void testMultipleHeaders()
{
    Url  *up;
    char url[128];

    //  Test request with multiple custom headers
    up = urlAlloc(0);

    teqi(urlFetch(up, "GET", SFMT(url, "%s/test/success", HTTP), NULL, 0,
                  "X-Custom-1: value1\r\nX-Custom-2: value2\r\n"), 200);

    urlFree(up);
}

static void testLongHeaderValues()
{
    Url  *up;
    char url[128], *longValue, *header;

    //  Test header with long value (but within limits)
    up = urlAlloc(0);

    longValue = rAlloc(500);
    memset(longValue, 'A', 499);
    longValue[499] = '\0';

    header = sfmt("X-Long-Header: %s\r\n", longValue);
    teqi(urlFetch(up, "GET", SFMT(url, "%s/test/success", HTTP), NULL, 0, header), 200);
    rFree(header);

    rFree(longValue);
    urlFree(up);
}

static void testStandardHeaders()
{
    Url   *up;
    char  url[128];
    cchar *date;

    //  Verify standard response headers are present
    up = urlAlloc(0);

    teqi(urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL), 200);

    //  Date header should be present
    date = urlGetHeader(up, "Date");
    tnotnull(date);

    urlFree(up);
}

static void testContentTypeVariations()
{
    Url   *up;
    char  url[128];
    cchar *contentType;

    up = urlAlloc(0);

    //  HTML file
    teqi(urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL), 200);
    contentType = urlGetHeader(up, "Content-Type");
    tnotnull(contentType);
    tnotnull(scontains(contentType, "text/html"));

    //  JSON endpoint
    urlClose(up);
    teqi(urlFetch(up, "GET", SFMT(url, "%s/test/success", HTTP), NULL, 0, NULL), 200);
    contentType = urlGetHeader(up, "Content-Type");
    tnotnull(contentType);
    tnotnull(scontains(contentType, "text/plain"));

    urlFree(up);
}

static void testCacheHeaders()
{
    Url   *up;
    char  url[128];
    cchar *etag, *lastModified;

    //  Static files should have caching headers
    up = urlAlloc(0);

    teqi(urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL), 200);

    etag = urlGetHeader(up, "ETag");
    lastModified = urlGetHeader(up, "Last-Modified");

    //  At least ETag and Last-Modified should be present for static files
    tnotnull(etag);
    tnotnull(lastModified);

    urlFree(up);
}

static void testConnectionHeader()
{
    Url   *up;
    char  url[128];
    cchar *connection;

    up = urlAlloc(0);

    //  Test keep-alive connection
    teqi(urlFetch(up, "GET", SFMT(url, "%s/test/success", HTTP), NULL, 0,
                  "Connection: keep-alive\r\n"), 200);
    connection = urlGetHeader(up, "Connection");
    tnotnull(connection);
    tmatch(connection, "keep-alive");

    //  Test close connection
    urlClose(up);
    teqi(urlFetch(up, "GET", SFMT(url, "%s/test/success", HTTP), NULL, 0,
                  "Connection: close\r\n"), 200);
    connection = urlGetHeader(up, "Connection");
    tnotnull(connection);
    tmatch(connection, "close");

    urlFree(up);
}

static void testHeaderCaseInsensitivity()
{
    Url   *up;
    char  url[128];
    cchar *value1, *value2, *value3;

    //  HTTP headers should be case-insensitive
    up = urlAlloc(0);

    teqi(urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL), 200);

    value1 = urlGetHeader(up, "Content-Type");
    value2 = urlGetHeader(up, "content-type");
    value3 = urlGetHeader(up, "CONTENT-TYPE");

    tnotnull(value1);
    tmatch(value1, value2);
    tmatch(value1, value3);

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        checkResponseHeaders();
        setHeaders();
        testMultipleHeaders();
        testLongHeaderValues();
        testStandardHeaders();
        testContentTypeVariations();
        testCacheHeaders();
        testConnectionHeader();
        testHeaderCaseInsensitivity();
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
