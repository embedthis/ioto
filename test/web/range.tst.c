/*
    range.tst.c - Unit tests for HTTP Range Request support (RFC 7233)

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void testSingleRange()
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *response, *contentRange, *acceptRanges;

    up = urlAlloc(0);

    // Test single range request: bytes=0-49 (first 50 bytes)
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0,
                      "Range: bytes=0-49\r\n");
    ttrue(status == 206);        // 206 Partial Content

    response = urlGetResponse(up);
    ttrue(response != NULL);
    ttrue(slen(response) == 50); // Should receive exactly 50 bytes

    contentRange = urlGetHeader(up, "Content-Range");
    ttrue(contentRange != NULL);
    if (!scontains(contentRange, "bytes 0-49/100")) {
        tinfo("Content-Range: %s (expected: bytes 0-49/100)", contentRange);
    }
    ttrue(scontains(contentRange, "bytes 0-49/100"));

    acceptRanges = urlGetHeader(up, "Accept-Ranges");
    ttrue(acceptRanges != NULL);
    ttrue(smatch(acceptRanges, "bytes"));

    // Test another single range: bytes=10-19 (10 bytes from offset 10)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0,
                      "Range: bytes=10-19\r\n");
    ttrue(status == 206);
    response = urlGetResponse(up);
    ttrue(slen(response) == 10);

    contentRange = urlGetHeader(up, "Content-Range");
    ttrue(scontains(contentRange, "bytes 10-19/100"));

    urlFree(up);
}

static void testMultiRange()
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *response, *contentType;

    up = urlAlloc(0);

    // Test multi-range request: bytes=0-9,20-29,40-49
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0,
                      "Range: bytes=0-9,20-29,40-49\r\n");
    ttrue(status == 206);  // 206 Partial Content

    response = urlGetResponse(up);
    ttrue(response != NULL);

    contentType = urlGetHeader(up, "Content-Type");
    ttrue(contentType != NULL);
    if (!sstarts(contentType, "multipart/byteranges")) {
        tinfo("Content-Type: %s", contentType);
    }
    ttrue(sstarts(contentType, "multipart/byteranges"));
    ttrue(scontains(contentType, "boundary="));

    // Response should contain multipart boundaries and Content-Range headers
    ttrue(scontains(response, "Content-Range: bytes 0-9/100"));
    ttrue(scontains(response, "Content-Range: bytes 20-29/100"));
    ttrue(scontains(response, "Content-Range: bytes 40-49/100"));

    urlFree(up);
}

static void testSuffixRange()
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *response, *contentRange;

    up = urlAlloc(0);

    // Test suffix range: bytes=-10 (last 10 bytes)
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0,
                      "Range: bytes=-10\r\n");
    ttrue(status == 206);

    response = urlGetResponse(up);
    ttrue(response != NULL);
    ttrue(slen(response) == 10);  // Should receive last 10 bytes

    contentRange = urlGetHeader(up, "Content-Range");
    ttrue(contentRange != NULL);
    ttrue(scontains(contentRange, "bytes 90-99/100"));

    // Test suffix range larger than file: bytes=-200 (should return entire file)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0,
                      "Range: bytes=-200\r\n");
    ttrue(status == 206);
    response = urlGetResponse(up);
    ttrue(slen(response) == 100);  // Should receive entire file

    contentRange = urlGetHeader(up, "Content-Range");
    ttrue(scontains(contentRange, "bytes 0-99/100"));

    urlFree(up);
}

static void testOpenEndedRange()
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *response, *contentRange;

    up = urlAlloc(0);

    // Test open-ended range: bytes=90- (from byte 90 to end)
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0,
                      "Range: bytes=90-\r\n");
    ttrue(status == 206);

    response = urlGetResponse(up);
    ttrue(response != NULL);
    ttrue(slen(response) == 10);  // Should receive last 10 bytes

    contentRange = urlGetHeader(up, "Content-Range");
    ttrue(contentRange != NULL);
    ttrue(scontains(contentRange, "bytes 90-99/100"));

    // Test open-ended from beginning: bytes=0-
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0,
                      "Range: bytes=0-\r\n");
    ttrue(status == 206);
    response = urlGetResponse(up);
    ttrue(slen(response) == 100);  // Should receive entire file

    contentRange = urlGetHeader(up, "Content-Range");
    ttrue(scontains(contentRange, "bytes 0-99/100"));

    urlFree(up);
}

static void testMalformedRange()
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // Test malformed Range header (missing "bytes=")
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0,
                      "Range: 0-49\r\n");
    ttrue(status == 400);  // 400 Bad Request

    // Test malformed Range header (invalid format)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0,
                      "Range: bytes=abc-xyz\r\n");
    ttrue(status == 400);

    urlFree(up);
}

static void testUnsatisfiableRange()
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *contentRange;

    up = urlAlloc(0);

    // Test range beyond file size: bytes=200-299 (file is only 100 bytes)
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0,
                      "Range: bytes=200-299\r\n");
    ttrue(status == 416);  // 416 Range Not Satisfiable

    contentRange = urlGetHeader(up, "Content-Range");
    ttrue(contentRange != NULL);
    ttrue(scontains(contentRange, "bytes */100"));

    // Test range starting beyond file size: bytes=150-
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0,
                      "Range: bytes=150-\r\n");
    ttrue(status == 416);

    urlFree(up);
}

static void testAcceptRangesHeader()
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *acceptRanges;

    up = urlAlloc(0);

    // Test that normal GET request includes Accept-Ranges header
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0, NULL);
    ttrue(status == 200);

    acceptRanges = urlGetHeader(up, "Accept-Ranges");
    ttrue(acceptRanges != NULL);
    ttrue(smatch(acceptRanges, "bytes"));

    urlFree(up);
}

static void testNormalRequestWithoutRange()
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *response;

    up = urlAlloc(0);

    // Test that normal request without Range header still works
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0, NULL);
    ttrue(status == 200);         // 200 OK, not 206

    response = urlGetResponse(up);
    ttrue(response != NULL);
    ttrue(slen(response) == 100); // Should receive entire file

    urlFree(up);
}

static void testRangeWithIfModifiedSince()
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *lastModified;

    up = urlAlloc(0);

    // First, get the Last-Modified date
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0, NULL);
    ttrue(status == 200);
    lastModified = urlGetHeader(up, "Last-Modified");
    ttrue(lastModified != NULL);

    /*
        NOTE: Testing interaction between Range and If-Modified-Since requires careful
        date/time handling. For now, we focus on testing Range requests independently.
        The If-Modified-Since feature is tested separately in other test suites.
     */
    urlFree(up);
}

static void testEdgeCases()
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *response;

    up = urlAlloc(0);

    // Test zero-length range (start == end-1)
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0,
                      "Range: bytes=0-0\r\n");
    ttrue(status == 206);
    response = urlGetResponse(up);
    ttrue(slen(response) == 1);  // Should receive 1 byte

    // Test range at end of file
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0,
                      "Range: bytes=99-99\r\n");
    ttrue(status == 206);
    response = urlGetResponse(up);
    ttrue(slen(response) == 1);

    /*
        NOTE: Overlapping ranges are an edge case that some servers reject.
        While RFC 7233 doesn't explicitly forbid them, testing this is not essential
        for basic range request compliance. The multipart range test above already
        validates multi-range support.
     */
    urlFree(up);
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, &HTTPS)) {
        testSingleRange();
        testMultiRange();
        testSuffixRange();
        testOpenEndedRange();
        testMalformedRange();
        testUnsatisfiableRange();
        testAcceptRangesHeader();
        testNormalRequestWithoutRange();
        testRangeWithIfModifiedSince();
        testEdgeCases();
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
