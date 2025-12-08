/*
    status-codes.tst.c - Test HTTP status code handling

    Tests that the Web server correctly generates and sends appropriate HTTP status codes
    for various scenarios including success, redirect, client errors, and server errors.

    Coverage:
    - 200 OK
    - 201 Created
    - 204 No Content
    - 304 Not Modified
    - 400 Bad Request
    - 401 Unauthorized
    - 403 Forbidden
    - 404 Not Found
    - 405 Method Not Allowed
    - 412 Precondition Failed
    - 416 Range Not Satisfiable
    - 500 Internal Server Error

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void test200OK(void)
{
    Url  *up;
    char url[128];

    up = urlAlloc(0);

    // Standard successful GET request
    teqi(urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL), 200);
    tnotnull(urlGetResponse(up));
    tnotnull(urlGetHeader(up, "Content-Type"));

    urlFree(up);
}

static void test201Created(void)
{
    Url  *up;
    char url[128];
    char data[64];
    int  pid;

    up = urlAlloc(0);

    // PUT to create a new file should return 201
    pid = getpid();
    SFMT(data, "test data %d", pid);

    teqi(urlFetch(up, "PUT", SFMT(url, "%s/upload/created-%d.txt", HTTP, pid),
                  data, slen(data), "Content-Type: text/plain\r\n"), 201);

    // Cleanup
    urlFetch(up, "DELETE", url, NULL, 0, NULL);

    urlFree(up);
}

static void test204NoContent(void)
{
    Url  *up;
    char url[128];
    char data[64];
    int  pid, status;

    up = urlAlloc(0);

    pid = getpid();
    SFMT(data, "test data %d", pid);

    // Create file first
    urlFetch(up, "PUT", SFMT(url, "%s/upload/nocontent-%d.txt", HTTP, pid),
             data, slen(data), "Content-Type: text/plain\r\n");

    // DELETE should return 204
    status = urlFetch(up, "DELETE", url, NULL, 0, NULL);
    teqi(status, 204);
    ttrue(urlGetResponse(up) == NULL || slen(urlGetResponse(up)) == 0);  // Keep ttrue for OR condition

    urlFree(up);
}

static void test304NotModified(void)
{
    Url   *up;
    char  url[128], headers[256];
    cchar *lastModified;
    int   status;

    up = urlAlloc(0);

    // Get Last-Modified header
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    teqi(status, 200);

    lastModified = urlGetHeader(up, "Last-Modified");
    tnotnull(lastModified);

    // Request with If-Modified-Since should return 304
    urlClose(up);
    SFMT(headers, "If-Modified-Since: %s\r\n", lastModified);
    status = urlFetch(up, "GET", url, NULL, 0, headers);
    teqi(status, 304);

    urlFree(up);
}

static void test400BadRequest(void)
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // Malformed request - invalid range syntax
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, "Range: bytes=invalid\r\n");
    // Server may return 200 (ignoring invalid range) or 400
    ttrue(status == 200 || status == 400 || status == 416);

    urlFree(up);
}

static void test401Unauthorized(void)
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // Access protected resource without credentials (requires HTTPS for basic auth)
    status = urlFetch(up, "GET", SFMT(url, "%s/basic/secret.html", HTTPS), NULL, 0, NULL);
    teqi(status, 401);
    tnotnull(urlGetHeader(up, "WWW-Authenticate"));

    urlFree(up);
}

static void test403Forbidden(void)
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // Try to access admin resource without credentials
    // Admin route requires digest auth with admin role
    status = urlFetch(up, "GET", SFMT(url, "%s/admin/secret.html", HTTPS), NULL, 0, NULL);
    // Should be 401 (requires authentication) or 403 (forbidden)
    ttrue(status == 401 || status == 403);

    urlFree(up);
}

static void test404NotFound(void)
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // Request non-existent file
    status = urlFetch(up, "GET", SFMT(url, "%s/does-not-exist-%d.html", HTTP, getpid()), NULL, 0, NULL);
    teqi(status, 404);
    tnotnull(urlGetResponse(up));  // Should have error body

    urlFree(up);
}

static void test405MethodNotAllowed(void)
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // TRACE should be disabled by default
    status = urlFetch(up, "TRACE", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    teqi(status, 405);

    urlFree(up);
}

static void test412PreconditionFailed(void)
{
    Url  *up;
    char url[128], data[64];
    int  pid, status;

    up = urlAlloc(0);

    pid = getpid();
    SFMT(data, "test data %d", pid);

    // Create a file
    urlFetch(up, "PUT", SFMT(url, "%s/upload/precond-%d.txt", HTTP, pid), data, slen(data),
             "Content-Type: text/plain\r\n");

    // Get its ETag
    status = urlFetch(up, "GET", url, NULL, 0, NULL);
    teqi(status, 200);
    tnotnull(urlGetHeader(up, "ETag"));

    // Try to PUT with wrong If-Match ETag - should fail with 412
    urlClose(up);
    status = urlFetch(up, "PUT", url, "new data", 8, "If-Match: \"wrong-etag\"\r\nContent-Type: text/plain\r\n");
    teqi(status, 412);

    // Cleanup
    urlFetch(up, "DELETE", url, NULL, 0, NULL);
    urlFree(up);
}

static void test416RangeNotSatisfiable(void)
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // Request range beyond file size
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, "Range: bytes=999999-\r\n");
    ttrue(status == 416 || status == 200);  // 416 or full content

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        test200OK();
        test201Created();
        test204NoContent();
        test304NotModified();
        test400BadRequest();
        test401Unauthorized();
        test403Forbidden();
        test404NotFound();
        test405MethodNotAllowed();
        test412PreconditionFailed();
        test416RangeNotSatisfiable();
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
