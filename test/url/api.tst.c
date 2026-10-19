/*
    api.tst.c - Unit tests for API argument guards and misuse

    The module documents itself as null tolerant. These tests hold every public entry point to
    that contract and cover the misuse paths that a well behaved caller never reaches.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char    *HTTP;
static char    *HTTPS;

/************************************ Code ************************************/

/*
    Every accessor must tolerate a NULL Url
 */
static void testNullUrlAccessors()
{
    char buf[64];

    ttrue(urlGetStatus(NULL) == R_ERR_BAD_ARGS);
    ttrue(urlGetResponse(NULL) == NULL);
    ttrue(urlGetResponseBuf(NULL) == NULL);
    ttrue(urlGetHeader(NULL, "content-type") == NULL);
    ttrue(urlGetCookie(NULL, "session") == NULL);
    ttrue(urlGetError(NULL) == NULL);
    ttrue(urlRead(NULL, buf, sizeof(buf)) == R_ERR_BAD_ARGS);
    ttrue(urlWrite(NULL, "abc", 3) == R_ERR_BAD_ARGS);
    ttrue(urlWriteFmt(NULL, "%s", "abc") == R_ERR_BAD_ARGS);
    ttrue(urlWriteFile(NULL, "data/test1.txt") == R_ERR_BAD_ARGS);
    ttrue(urlWriteHeaders(NULL, "X-Test: 1\r\n") == R_ERR_BAD_ARGS);
    ttrue(urlFinalize(NULL) == R_ERR_BAD_ARGS);
    ttrue(urlParse(NULL, "http://localhost/") == R_ERR_BAD_ARGS);
    ttrue(urlUpload(NULL, NULL, NULL, NULL) == R_ERR_BAD_ARGS);
    ttrue(urlStart(NULL, "GET", "http://localhost/") == R_ERR_BAD_ARGS);
}

/*
    urlFetch and urlJson accept a NULL Url and allocate a temporary one for the request
 */
static void testFetchWithoutUrl()
{
    char url[128];

    ttrue(urlFetch(NULL, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL) == 200);
    ttrue(urlFetch(NULL, "GET", SFMT(url, "%s/missing-document.html", HTTP), NULL, 0, NULL) == 404);
}

/*
    The setters must be no-ops on a NULL Url, not a crash
 */
static void testNullUrlSetters()
{
    urlClose(NULL);
    urlFree(NULL);
    urlSetFlags(NULL, 0);
    urlSetBufLimit(NULL, 1024);
    urlSetTimeout(NULL, TPS);
    urlSetStatus(NULL, 200);
    urlSetProtocol(NULL, 1);
    urlSetMaxRetries(NULL, 3);
    urlSetCerts(NULL, NULL, NULL, NULL, NULL);
    urlSetCiphers(NULL, "HIGH");
    urlSetVerify(NULL, 1, 1);
    urlSetAuth(NULL, "user", "password", "basic");
    ttrue(urlError(NULL, "ignored") == R_ERR_BAD_ARGS);
    ttrue(1);
}

/*
    Missing method or url is bad arguments, not a connection attempt
 */
static void testMissingArgs()
{
    Url *up;

    up = urlAlloc(0);
    ttrue(urlStart(up, NULL, "http://localhost/") == R_ERR_BAD_ARGS);
    ttrue(urlStart(up, "GET", NULL) == R_ERR_BAD_ARGS);
    ttrue(urlParse(up, NULL) == R_ERR_BAD_ARGS);
    ttrue(urlFetch(up, NULL, "http://localhost/", NULL, 0, NULL) == R_ERR_BAD_ARGS);
    ttrue(urlFetch(up, "GET", NULL, NULL, 0, NULL) == R_ERR_BAD_ARGS);
    urlFree(up);

    ttrue(urlGet(NULL, NULL) == NULL);
    ttrue(urlPost(NULL, "abc", 3, NULL) == NULL);
    ttrue(urlGetEvents(NULL, NULL, NULL, NULL) == R_ERR_BAD_ARGS);
}

/*
    urlSseRun requires both a Url and a callback
 */
static void testSseRunArgs()
{
    Url *up;

    up = urlAlloc(0);
    ttrue(urlSseRun(NULL, NULL, NULL, NULL, 0) == R_ERR_BAD_ARGS);
    ttrue(urlSseRun(up, NULL, NULL, NULL, 0) == R_ERR_BAD_ARGS);
    urlFree(up);
}

/*
    Writing after finalizing the request is a caller error
 */
static void testWriteAfterFinalize()
{
    Url  *up;
    char url[128];

    up = urlAlloc(0);
    ttrue(urlStart(up, "POST", SFMT(url, "%s/test/show", HTTP)) == 0);
    ttrue(urlWrite(up, "hello", 5) == 5);
    ttrue(urlFinalize(up) == 0);

    //  The request is complete, so a further write must be refused
    ttrue(urlWrite(up, "more", 4) < 0);
    tcontains(urlGetError(up), "Cannot write after finalize");
    urlFree(up);
}

/*
    urlWrite computes the length of a string body when passed a zero size
 */
static void testWriteStringLength()
{
    Url   *up;
    Json  *json;
    char  url[128];

    up = urlAlloc(0);
    ttrue(urlStart(up, "POST", SFMT(url, "%s/test/show", HTTP)) == 0);
    ttrue(urlWrite(up, "measured", 0) == (ssize) slen("measured"));
    ttrue(urlFinalize(up) == 0);
    ttrue(urlGetStatus(up) == 200);

    json = urlGetJsonResponse(up);
    ttrue(json != NULL);
    if (json) {
        tmatch(jsonGet(json, 0, "body", 0), "measured");
        jsonFree(json);
    }
    urlFree(up);
}

/*
    urlWriteFmt formats the body
 */
static void testWriteFmt()
{
    Url   *up;
    Json  *json;
    char  url[128];

    up = urlAlloc(0);
    ttrue(urlStart(up, "POST", SFMT(url, "%s/test/show", HTTP)) == 0);
    ttrue(urlWriteFmt(up, "count=%d name=%s", 42, "widget") > 0);
    ttrue(urlFinalize(up) == 0);

    json = urlGetJsonResponse(up);
    ttrue(json != NULL);
    if (json) {
        tmatch(jsonGet(json, 0, "body", 0), "count=42 name=widget");
        jsonFree(json);
    }
    urlFree(up);
}

/*
    urlWriteFile streams a file as the request body
 */
static void testWriteFile()
{
    Url   *up;
    Json  *json;
    char  url[128];

    up = urlAlloc(0);
    ttrue(urlStart(up, "POST", SFMT(url, "%s/test/show", HTTP)) == 0);
    ttrue(urlWriteFile(up, "site/data/test1.txt") == 30);
    ttrue(urlFinalize(up) == 0);
    ttrue(urlGetStatus(up) == 200);

    json = urlGetJsonResponse(up);
    ttrue(json != NULL);
    if (json) {
        ttrue(jsonGet(json, 0, "body", 0) != NULL);
        jsonFree(json);
    }
    urlFree(up);
}

/*
    A missing body file must be reported, not silently sent as an empty body
 */
static void testWriteMissingFile()
{
    Url  *up;
    char url[128];

    up = urlAlloc(0);
    ttrue(urlStart(up, "POST", SFMT(url, "%s/test/show", HTTP)) == 0);
    ttrue(urlWriteFile(up, "site/data/no-such-file.txt") < 0);
    ttrue(urlGetError(up) != NULL);
    urlFree(up);
}

/*
    A caller supplied Content-Length must be honoured instead of chunking
 */
static void testExplicitContentLength()
{
    Url   *up;
    Json  *json;
    char  url[128];

    up = urlAlloc(0);
    ttrue(urlStart(up, "POST", SFMT(url, "%s/test/show", HTTP)) == 0);
    ttrue(urlWriteHeaders(up, "Content-Length: 5\r\n") == 0);
    ttrue(urlWrite(up, "12345", 5) == 5);
    ttrue(urlFinalize(up) == 0);
    ttrue(urlGetStatus(up) == 200);

    json = urlGetJsonResponse(up);
    ttrue(json != NULL);
    if (json) {
        tmatch(jsonGet(json, 0, "body", 0), "12345");
        tmatch(jsonGet(json, 0, "contentLength", 0), "5");
        jsonFree(json);
    }
    urlFree(up);
}

/*
    urlWriteHeaders is idempotent -- a second call must not resend the headers
 */
static void testWriteHeadersTwice()
{
    Url  *up;
    char url[128];

    up = urlAlloc(0);
    ttrue(urlStart(up, "GET", SFMT(url, "%s/index.html", HTTP)) == 0);
    ttrue(urlWriteHeaders(up, "X-First: 1\r\n") == 0);
    ttrue(urlWriteHeaders(up, "X-Second: 2\r\n") == 0);
    ttrue(urlFinalize(up) == 0);
    ttrue(urlGetStatus(up) == 200);
    urlFree(up);
}

/*
    A cookie that was never set must not be reported, and a cookie whose name is a prefix of
    another must not match it
 */
static void testMissingCookie()
{
    Url  *up;
    char *cookie;
    char url[128];

    up = urlAlloc(0);
    ttrue(urlFetch(up, "GET", SFMT(url, "%s/test/cookie?name=sessionid&value=abc123", HTTP), NULL, 0, NULL) == 200);

    cookie = urlGetCookie(up, "sessionid");
    tmatch(cookie, "abc123");
    rFree(cookie);

    //  "session" is a prefix of "sessionid" and must not match
    ttrue(urlGetCookie(up, "session") == NULL);
    ttrue(urlGetCookie(up, "absent") == NULL);
    urlFree(up);

    //  A response with no Set-Cookie header at all
    up = urlAlloc(0);
    ttrue(urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL) == 200);
    ttrue(urlGetCookie(up, "sessionid") == NULL);
    urlFree(up);
}

/*
    urlClose releases the connection but leaves the object usable for a further request
 */
static void testCloseAndReuse()
{
    Url  *up;
    char url[128];

    up = urlAlloc(0);
    ttrue(urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL) == 200);
    urlClose(up);

    ttrue(urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL) == 200);
    tcontains(urlGetResponse(up), "Hello /index.html");
    urlClose(up);

    //  Closing twice must be harmless
    urlClose(up);
    urlFree(up);
}

/*
    urlUpload with a file that does not exist must fail rather than send a truncated body
 */
static void testUploadMissingFile()
{
    Url   *up;
    RList *files;
    char  url[128];

    up = urlAlloc(0);
    ttrue(urlStart(up, "POST", SFMT(url, "%s/test/upload", HTTP)) == 0);

    files = rAllocList(0, 0);
    rAddItem(files, "site/data/no-such-upload.txt");
    ttrue(urlUpload(up, files, NULL, NULL) < 0);
    ttrue(urlGetError(up) != NULL);

    rFreeList(files);
    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testNullUrlAccessors();
        testFetchWithoutUrl();
        testNullUrlSetters();
        testMissingArgs();
        testSseRunArgs();
        testWriteAfterFinalize();
        testWriteStringLength();
        testWriteFmt();
        testWriteFile();
        testWriteMissingFile();
        testExplicitContentLength();
        testWriteHeadersTwice();
        testMissingCookie();
        testCloseAndReuse();
        testUploadMissingFile();
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
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
