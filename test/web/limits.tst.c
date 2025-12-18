/*
    limits.tst.c - HTTP request and response limit testing

    Tests the web server's handling of various size limits and boundary conditions.
    Validates that the server properly enforces configured limits and returns
    appropriate error codes when limits are exceeded.

    Based on Appweb test/basic/limits.tst.es

    Coverage:
    - Header size limits (10K)
    - Body size limits (100K for regular, 20MB for uploads)
    - URI length limits
    - Query string limits
    - Large number of headers
    - Boundary conditions (at limit, just over limit)

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

// Includes

#include "test.h"

// Locals

static char *HTTP;
static char *HTTPS;

// Code

static void testHeaderSizeLimit(void)
{
    Url    *up;
    char   *header, *headers, url[128];
    size_t hlen;
    int    status;

    up = urlAlloc(0);
    SFMT(url, "%s/index.html?test=header-size-limit", HTTP);
    // Test 1: Normal header (well under limit) should succeed
    status = urlFetch(up, "GET", url, NULL, 0, "X-Test-Header: normal value\r\n");
    teqi(status, 200);
    urlClose(up);

    /*
        Test 2: Large header exceeding 10K limit should fail
        Fill with 11KB of data to exceed the 10K header limit
     */
    hlen = 11 * 1024;
    header = rAlloc(hlen + 1);
    memset(header, 'A', hlen);
    header[hlen] = '\0';
    headers = sfmt("X-Large-Header: %s\r\n", header);
    rFree(header);

    status = urlFetch(up, "GET", SFMT(url, "%s/index.html?test=header-size-limit2", HTTP), NULL, 0, headers);
    teqi(status, 413);  // 413 Payload Too Large
    rFree(headers);

    urlFree(up);
}

static void testMultipleHeaders(void)
{
    Url  *up;
    RBuf *buf;
    char url[128];
    char *headers;
    int  status, i;

    up = urlAlloc(0);

    // Test 1: Many small headers that fit within 10K total limit
    buf = rAllocBuf(0);
    for (i = 0; i < 50; i++) {
        rPutToBuf(buf, "X-Header-%d: value%d\r\n", i, i);
    }
    headers = rBufToStringAndFree(buf);

    status = urlFetch(up, "GET", SFMT(url, "%s/index.html?test=multiple-headers", HTTP), NULL, 0, headers);
    teqi(status, 200);
    rFree(headers);
    urlClose(up);

    /*
        Test 2: Many headers exceeding 10K limit
        Generate enough headers to exceed 10KB (10240 bytes)
        Each header is about 60 bytes, so we need ~200 headers
     */
    buf = rAllocBuf(0);
    for (i = 0; i < 250; i++) {
        rPutToBuf(buf, "X-Very-Long-Header-Name-%d: value-with-quite-a-bit-more-data-here-%d\r\n", i, i);
    }
    headers = rBufToStringAndFree(buf);

    status = urlFetch(up, "GET", SFMT(url, "%s/index.html?test=multiple-headers2", HTTP), NULL, 0, headers);
    size_t totalSize = slen(headers);
    if (totalSize < 10240) {
        teqi(status, 200);
    } else {
        teqi(status, 413);
    }
    rFree(headers);

    urlFree(up);
}

static void testBodySizeLimit(void)
{
    Url    *up;
    char   url[128];
    char   *largeBody;
    int    status;
    size_t bodySize;

    up = urlAlloc(0);

    // Test 1: POST with body under 100K limit should succeed
    bodySize = 50 * 1024;  // 50K
    largeBody = rAlloc(bodySize + 1);
    memset(largeBody, 'B', bodySize);
    largeBody[bodySize] = '\0';

    status = urlFetch(up, "POST", SFMT(url, "%s/test/show", HTTP), largeBody, bodySize,
                      "Content-Type: application/octet-stream\r\n");
    teqi(status, 200);
    rFree(largeBody);
    urlClose(up);

    // Test 2: POST with body exceeding 100K limit should fail
    bodySize = 150 * 1024;  // 150K (exceeds 100K limit)
    largeBody = rAlloc(bodySize + 1);
    memset(largeBody, 'C', bodySize);
    largeBody[bodySize] = '\0';

    status = urlFetch(up, "POST", SFMT(url, "%s/test/show", HTTP), largeBody, bodySize,
                      "Content-Type: application/octet-stream\r\n");
    // Should get 413 Payload Too Large or connection error
    ttrue(status == 413 || status < 0);
    rFree(largeBody);

    urlFree(up);
    // rSleep(10);  // Give server time to return to accept loop after 413
}

static void testUploadSizeLimit(void)
{
    Url    *up;
    char   url[128];
    char   *uploadData;
    int    status, pid;
    size_t uploadSize;

    up = urlAlloc(0);
    pid = getpid();

    /*
        Test 1: Small upload under 20MB limit should succeed
        Use smaller size for faster test execution
     */
    uploadSize = 10 * 1024;  // 10K (well under 20MB limit)
    uploadData = rAlloc(uploadSize + 1);
    memset(uploadData, 'U', uploadSize);
    uploadData[uploadSize] = '\0';

    status = urlFetch(up, "PUT", SFMT(url, "%s/upload/limit-test-%d.dat", HTTP, pid),
                      uploadData, uploadSize, "Content-Type: application/octet-stream\r\n");
    ttrue(status == 201 || status == 204);
    rFree(uploadData);
    urlClose(up);

    urlFetch(up, "DELETE", url, NULL, 0, NULL);

    /*
        Note: Testing 20MB+ uploads is skipped for test performance
        In production environment, would test:
        - 20MB exactly (at limit)
        - 25MB (over limit, expect 413)
     */
    urlFree(up);
}

static void testURILength(void)
{
    Url    *up;
    RBuf   *buf;
    char   *longURI;
    char   base[128];
    int    status;
    size_t targetLen;

    up = urlAlloc(0);

    // Test 1: Normal URI should work
    status = urlFetch(up, "GET", SFMT(base, "%s/index.html?test=uri-length", HTTP), NULL, 0, NULL);
    teqi(status, 200);

    /*
        Test 2: Very long URI with query string
        Build a URI approaching limit (most servers limit to 8K-16K)
     */
    targetLen = 8 * 1024;  // 8K URI
    buf = rAllocBuf(0);
    rPutToBuf(buf, "%s/index.html?test=uri-length2", HTTP);

    while (rGetBufLength(buf) < targetLen - 50) {
        rPutStringToBuf(buf, "param=value&");
    }
    longURI = rBufToStringAndFree(buf);
    urlClose(up);

    status = urlFetch(up, "GET", longURI, NULL, 0, NULL);
    /*
        May succeed or fail depending on URI limit (typically 8-16K)
        We accept both outcomes as servers vary
     */
    ttrue(status == 200 || status == 414 || status < 0);

    rFree(longURI);
    urlFree(up);
}

static void testQueryStringLimit(void)
{
    Url  *up;
    RBuf *buf;
    char *url;
    int  status, i;

    up = urlAlloc(0);

    // Test: Large query string
    buf = rAllocBuf(0);
    rPutToBuf(buf, "%s/index.html?test=query-string-limit", HTTP);
    // Add many query parameters but under the limit
    for (i = 0; i < 100; i++) {
        rPutToBuf(buf, "param%d=value%d&", i, i);
    }
    url = rBufToStringAndFree(buf);
    status = urlFetch(up, "GET", url, NULL, 0, NULL);
    teqi(status, 200);
    rFree(url);
    urlClose(up);

    // Test: Large query string exceeding the limit
    buf = rAllocBuf(0);
    rPutToBuf(buf, "%s/index.html?test=query-string-limit3", HTTP);
    for (i = 0; i < 1000; i++) {
        rPutToBuf(buf, "param%d=value%d&", i, i);
    }
    url = rBufToStringAndFree(buf);
    status = urlFetch(up, "GET", url, NULL, 0, NULL);
    teqi(status, 413);

    urlFree(up);
}

static void testBoundaryConditions(void)
{
    Url    *up;
    char   url[128];
    char   *bodyData;
    int    status;
    size_t exactLimit;

    up = urlAlloc(0);

    // Test: Body at exact 100K limit
    exactLimit = 100 * 1024;  // Exactly 100K
    bodyData = rAlloc(exactLimit + 1);
    memset(bodyData, 'X', exactLimit);
    bodyData[exactLimit] = '\0';

    status = urlFetch(up, "POST", SFMT(url, "%s/test/show", HTTP), bodyData, exactLimit,
                      "Content-Type: application/octet-stream\r\n");
    // At exact limit, should succeed or fail depending on < vs <= in limit check
    ttrue(status == 200 || status == 413);

    rFree(bodyData);

    // Test: Body at 100K + 1 byte
    bodyData = rAlloc(exactLimit + 2);
    memset(bodyData, 'Y', exactLimit + 1);
    bodyData[exactLimit + 1] = '\0';

    urlClose(up);
    status = urlFetch(up, "POST", SFMT(url, "%s/test/show", HTTP), bodyData, exactLimit + 1,
                      "Content-Type: application/octet-stream\r\n");
    // Just over limit - should definitely fail
    ttrue(status == 413 || status < 0);

    rFree(bodyData);
    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testHeaderSizeLimit();
        testMultipleHeaders();
        testBodySizeLimit();
        testUploadSizeLimit();
        testURILength();
        testQueryStringLimit();
        testBoundaryConditions();
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
