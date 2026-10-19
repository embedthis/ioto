/*
    conditional-delete.tst.c - RFC 7232 preconditions on DELETE

    conditional.tst.c covers If-Match on DELETE. These cover the remaining precondition paths:
    If-Unmodified-Since, and a precondition naming a document that does not exist.

    Each test works on a file it creates itself under /upload/, which is not tracked, so a failed
    run cannot leave a tracked fixture deleted.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

/*
    Create a document to operate on and return its URL path. Caller frees.
 */
static char *makeDocument(Url *up, cchar *tag)
{
    char *path;
    char url[160];
    int  status;

    path = sfmt("/upload/cond-%s-%d.txt", tag, getpid());
    urlClose(up);
    status = urlFetch(up, "PUT", SFMT(url, "%s%s", HTTP, path), "original", 8, NULL);
    //  201 when newly created, 204 when a previous run left one behind
    ttrue(status == 201 || status == 204);
    return path;
}

static int fetch(Url *up, cchar *method, cchar *path, cchar *headers)
{
    char url[160];

    urlClose(up);
    return urlFetch(up, method, SFMT(url, "%s%s", HTTP, path), NULL, 0, headers);
}

/*
    Send a request and assert its status. The request must be made exactly once: teqi evaluates
    its first argument twice, once to compare and once to report.
 */
static void checkFetch(Url *up, cchar *method, cchar *path, cchar *headers, int expect)
{
    int status;

    status = fetch(up, method, path, headers);
    teqi(status, expect);
}

/*
    If-Unmodified-Since asserts the client holds the current version. A date older than the
    document means it does not, so the delete must be refused and the document left alone.
 */
static void testIfUnmodifiedSinceFailure(void)
{
    Url  *up;
    char *path;

    up = urlAlloc(0);
    path = makeDocument(up, "unmod-fail");

    checkFetch(up, "DELETE", path, "If-Unmodified-Since: Mon, 01 Jan 2000 00:00:00 GMT\r\n", 412);

    //  The document survives a refused delete
    checkFetch(up, "GET", path, NULL, 200);

    //  Clean up
    checkFetch(up, "DELETE", path, NULL, 204);
    rFree(path);
    urlFree(up);
}

/*
    A date at or after the document's own Last-Modified satisfies the precondition.
 */
static void testIfUnmodifiedSinceSuccess(void)
{
    Url  *up;
    char *path, *headers;

    up = urlAlloc(0);
    path = makeDocument(up, "unmod-ok");

    checkFetch(up, "GET", path, NULL, 200);
    tnotnull(urlGetHeader(up, "Last-Modified"));

    headers = sfmt("If-Unmodified-Since: %s\r\n", "Tue, 01 Jan 2999 00:00:00 GMT");
    checkFetch(up, "DELETE", path, headers, 204);
    rFree(headers);

    //  It is gone
    checkFetch(up, "GET", path, NULL, 404);
    rFree(path);
    urlFree(up);
}

/*
    A precondition on a document that does not exist cannot be evaluated, so the request is a
    404 rather than a 412.
 */
static void testPreconditionOnMissingDocument(void)
{
    Url  *up;
    char path[160];

    up = urlAlloc(0);
    SFMT(path, "/upload/cond-absent-%d.txt", getpid());

    checkFetch(up, "DELETE", path, "If-Match: \"any-etag\"\r\n", 404);
    checkFetch(up, "DELETE", path, "If-Unmodified-Since: Mon, 01 Jan 2000 00:00:00 GMT\r\n", 404);

    //  And with no precondition at all
    checkFetch(up, "DELETE", path, NULL, 404);
    urlFree(up);
}

/*
    A correct ETag satisfies If-Match and the delete proceeds.
 */
static void testIfMatchSuccess(void)
{
    Url   *up;
    char  *path, *headers, *etag;

    up = urlAlloc(0);
    path = makeDocument(up, "match-ok");

    checkFetch(up, "GET", path, NULL, 200);
    etag = sclone(urlGetHeader(up, "ETag"));
    tnotnull(etag);

    headers = sfmt("If-Match: %s\r\n", etag);
    checkFetch(up, "DELETE", path, headers, 204);
    rFree(headers);
    rFree(etag);

    checkFetch(up, "GET", path, NULL, 404);
    rFree(path);
    urlFree(up);
}

/*
    If-Match: * requires only that the document exists.
 */
static void testIfMatchWildcard(void)
{
    Url  *up;
    char *path;

    up = urlAlloc(0);
    path = makeDocument(up, "match-wild");

    checkFetch(up, "DELETE", path, "If-Match: *\r\n", 204);
    checkFetch(up, "GET", path, NULL, 404);
    rFree(path);
    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
#if ME_WEB_MUTATIONS
        testIfUnmodifiedSinceFailure();
        testIfUnmodifiedSinceSuccess();
        testPreconditionOnMissingDocument();
        testIfMatchSuccess();
        testIfMatchWildcard();
#else
        tskip("PUT and DELETE are unavailable on this platform\n");
#endif
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
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
