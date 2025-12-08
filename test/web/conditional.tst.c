/*
    conditional.tst.c - Unit tests for HTTP Conditional Requests (RFC 7232)

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void testIfNoneMatchWithMatching()
{
    Url  *up;
    char url[128];
    int  status;
    char *etag, *headers;

    up = urlAlloc(0);

    //  First, get the file and its ETag
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0, NULL);
    teqi(status, 200);

    //  Clone ETag from response headers before closing
    etag = sclone(urlGetHeader(up, "ETag"));
    tnotnull(etag);

    //  Close and create new request with If-None-Match
    urlClose(up);

    //  Format headers using sfmt which returns allocated string
    headers = sfmt("If-None-Match: %s\r\n", etag);
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0, headers);
    teqi(status, 304);  // 304 Not Modified

    rFree(headers);
    rFree(etag);
    urlFree(up);
}

static void testIfNoneMatchWithDifferent()
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *response;

    up = urlAlloc(0);

    //  Request with If-None-Match using a different ETag - should get 200
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0,
                      "If-None-Match: \"different-etag\"\r\n");
    teqi(status, 200);         // 200 OK

    response = urlGetResponse(up);
    tnotnull(response);
    teqz(slen(response), 100); // Full content

    urlFree(up);
}

static void testIfNoneMatchWildcard()
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    //  Request with If-None-Match: * - should get 304 if resource exists
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0,
                      "If-None-Match: *\r\n");
    teqi(status, 304);  // 304 Not Modified

    urlFree(up);
}

static void testIfMatchSuccess()
{
    Url  *up;
    char url[128];
    int  status;
    char *etag, *headers;

    up = urlAlloc(0);

    //  First, get the file and its ETag
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test-write.txt", HTTP), NULL, 0, NULL);
    teqi(status, 200);
    etag = sclone(urlGetHeader(up, "ETag"));
    tnotnull(etag);

    //  Now PUT with If-Match using the correct ETag - should succeed
    urlClose(up);
    headers = sfmt("If-Match: %s\r\n", etag);
    status = urlFetch(up, "PUT", SFMT(url, "%s/range-test-write.txt", HTTP),
                      "Updated content", 15, headers);
    if (status != 204 && status != 201) {
        tinfo("PUT with If-Match failed: status=%d, etag=%s", status, etag);
    }
    ttrue(status == 204 || status == 201);  // Success

    rFree(headers);
    rFree(etag);
    urlFree(up);
}

static void testIfMatchFailure()
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    //  PUT with If-Match using wrong ETag - should get 412
    status = urlFetch(up, "PUT", SFMT(url, "%s/range-test-write.txt", HTTP),
                      "Updated content", 15,
                      "If-Match: \"wrong-etag\"\r\n");
    teqi(status, 412);  // 412 Precondition Failed

    urlFree(up);
}

static void testIfMatchWildcard()
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    //  PUT with If-Match: * - should succeed if resource exists
    status = urlFetch(up, "PUT", SFMT(url, "%s/range-test-write.txt", HTTP),
                      "Updated content", 15,
                      "If-Match: *\r\n");
    ttrue(status == 204 || status == 201);  // Success

    urlFree(up);
}

static void testIfModifiedSinceNotModified()
{
    Url  *up;
    char url[128];
    int  status;
    char *lastModified, *headers;

    up = urlAlloc(0);

    //  First, get the file and its Last-Modified date
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0, NULL);
    teqi(status, 200);
    lastModified = sclone(urlGetHeader(up, "Last-Modified"));
    tnotnull(lastModified);

    //  Now request with If-Modified-Since using the same date - should get 304
    urlClose(up);
    headers = sfmt("If-Modified-Since: %s\r\n", lastModified);
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0, headers);
    teqi(status, 304);  // 304 Not Modified

    rFree(headers);
    rFree(lastModified);
    urlFree(up);
}

static void testIfModifiedSinceModified()
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *response;

    up = urlAlloc(0);

    //  Request with If-Modified-Since using an old date - should get 200
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0,
                      "If-Modified-Since: Mon, 01 Jan 2000 00:00:00 GMT\r\n");
    teqi(status, 200);         // 200 OK

    response = urlGetResponse(up);
    tnotnull(response);
    teqz(slen(response), 100); // Full content

    urlFree(up);
}

static void testIfUnmodifiedSinceSuccess()
{
    Url  *up;
    char url[128], headers[256];
    int  status;
    char *lastModified;

    up = urlAlloc(0);

    //  First, get the file and its Last-Modified date
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test-write.txt", HTTP), NULL, 0, NULL);
    teqi(status, 200);
    lastModified = sclone(urlGetHeader(up, "Last-Modified"));
    tnotnull(lastModified);

    //  Now PUT with If-Unmodified-Since using same or future date - should succeed
    urlClose(up);
    status = urlFetch(up, "PUT", SFMT(url, "%s/range-test-write.txt", HTTP),
                      "Updated content", 15, SFMT(headers, "If-Unmodified-Since: %s\r\n", lastModified));
    ttrue(status == 204 || status == 201);  // Success

    rFree(lastModified);
    urlFree(up);
}

static void testIfUnmodifiedSinceFailure()
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    //  PUT with If-Unmodified-Since using old date - should get 412
    status = urlFetch(up, "PUT", SFMT(url, "%s/range-test-write.txt", HTTP),
                      "Updated content", 15,
                      "If-Unmodified-Since: Mon, 01 Jan 2000 00:00:00 GMT\r\n");
    teqi(status, 412);  // 412 Precondition Failed

    urlFree(up);
}

static void testIfNoneMatchPrecedence()
{
    Url   *up;
    char  url[128], headers[256];
    int   status;
    cchar *etag;

    up = urlAlloc(0);

    //  First, get the file and its ETag
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0, NULL);
    teqi(status, 200);
    etag = urlGetHeader(up, "ETag");
    tnotnull(etag);

    /*
        Send both If-None-Match (matching) and If-Modified-Since (old date)
        Per RFC 7232, If-None-Match takes precedence and should return 304
     */
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0,
                      SFMT(headers, "If-None-Match: %s\r\nIf-Modified-Since: Mon, 01 Jan 2000 00:00:00 GMT\r\n", etag));
    teqi(status, 304);  // 304 Not Modified (If-None-Match wins)

    urlFree(up);
}

static void testIfRangeWithMatchingEtag()
{
    Url   *up;
    char  url[128], headers[256];
    int   status;
    cchar *etag, *response;

    up = urlAlloc(0);

    //  First, get the file and its ETag
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0, NULL);
    teqi(status, 200);
    etag = urlGetHeader(up, "ETag");
    tnotnull(etag);

    //  Now request range with If-Range matching ETag - should get 206 with range
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0,
                      SFMT(headers, "Range: bytes=0-49\r\nIf-Range: %s\r\n", etag));
    teqi(status, 206);        // 206 Partial Content

    response = urlGetResponse(up);
    tnotnull(response);
    teqz(slen(response), 50); // Range content only

    urlFree(up);
}

static void testIfRangeWithDifferentEtag()
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *response;

    up = urlAlloc(0);

    //  Request range with If-Range using wrong ETag - should get 200 with full content
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0,
                      "Range: bytes=0-49\r\nIf-Range: \"wrong-etag\"\r\n");
    teqi(status, 200);         // 200 OK (full content, not range)

    response = urlGetResponse(up);
    tnotnull(response);
    teqz(slen(response), 100); // Full content, not range

    urlFree(up);
}

static void testIfRangeWithDate()
{
    Url   *up;
    char  url[128], headers[256];
    int   status;
    char  *lastModified;
    cchar *response;

    up = urlAlloc(0);

    //  First, get the file and its Last-Modified date
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0, NULL);
    teqi(status, 200);
    lastModified = sclone(urlGetHeader(up, "Last-Modified"));
    tnotnull(lastModified);

    //  Now request range with If-Range matching date - should get 206 with range
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0,
                      SFMT(headers, "Range: bytes=0-49\r\nIf-Range: %s\r\n", lastModified));
    teqi(status, 206);        // 206 Partial Content

    response = urlGetResponse(up);
    tnotnull(response);
    teqz(slen(response), 50); // Range content only

    rFree(lastModified);
    urlFree(up);
}

static void testMultipleETags()
{
    Url   *up;
    char  url[128], headers[256];
    int   status;
    cchar *etag;

    up = urlAlloc(0);

    //  First, get the file and its ETag
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0, NULL);
    teqi(status, 200);
    etag = urlGetHeader(up, "ETag");
    tnotnull(etag);

    //  Request with If-None-Match containing multiple ETags (including matching one)
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0,
                      SFMT(headers, "If-None-Match: \"other-etag\", %s, \"another-etag\"\r\n", etag));
    teqi(status, 304);  // 304 Not Modified (one of the ETags matched)

    urlFree(up);
}

static void testDeleteWithPrecondition()
{
    Url   *up;
    char  url[128];
    int   status;
    cchar *etag;

    up = urlAlloc(0);

    //  First, get the file and its ETag
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test-write.txt", HTTP), NULL, 0, NULL);
    teqi(status, 200);
    etag = urlGetHeader(up, "ETag");
    tnotnull(etag);

    //  DELETE with If-Match using wrong ETag - should get 412
    urlClose(up);
    status = urlFetch(up, "DELETE", SFMT(url, "%s/range-test-write.txt", HTTP), NULL, 0,
                      "If-Match: \"wrong-etag\"\r\n");
    teqi(status, 412);  // 412 Precondition Failed

    //  File should still exist
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test-write.txt", HTTP), NULL, 0, NULL);
    teqi(status, 200);  // File still exists

    urlFree(up);
}

static void testMalformedEtag()
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    //  Request with malformed If-None-Match (missing quotes) - should get 400
    status = urlFetch(up, "GET", SFMT(url, "%s/range-test.txt", HTTP), NULL, 0,
                      "If-None-Match: malformed-etag\r\n");
    teqi(status, 400);  // 400 Bad Request

    urlFree(up);
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, &HTTPS)) {
        testIfNoneMatchWithMatching();
        testIfNoneMatchWithDifferent();
        testIfNoneMatchWildcard();
        testIfMatchSuccess();
        testIfMatchFailure();
        testIfMatchWildcard();
        testIfModifiedSinceNotModified();
        testIfModifiedSinceModified();
        testIfUnmodifiedSinceSuccess();
        testIfUnmodifiedSinceFailure();
        testIfNoneMatchPrecedence();
        testIfRangeWithMatchingEtag();
        testIfRangeWithDifferentEtag();
        testIfRangeWithDate();
        testMultipleETags();
        testDeleteWithPrecondition();
        testMalformedEtag();
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
