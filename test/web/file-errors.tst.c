/*
    file-errors.tst.c - PUT and DELETE failure handling in the file handler

    The write paths of the file handler are where a refusal has side effects: a partial file left
    on disk, a directory unlinked, a body limit enforced only after the bytes were stored. Each
    failure here must answer with the documented status and leave the document root as it found
    it. Limit cases run on an in-process host with a 1K upload limit.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

#define SMALL "http://localhost:4861"

/************************************ Code ************************************/

static int fetch(cchar *base, cchar *method, cchar *path, cchar *body, size_t len)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);
    status = urlFetch(up, method, SFMT(url, "%s%s", base, path), body, len, NULL);
    urlFree(up);
    return status;
}

/*
    A PUT that declares more than the limit is refused on its headers; a chunked PUT that
    exceeds the limit is refused once the limit is crossed. Neither leaves a file.
 */
static void testPutOverLimit(void)
{
    WebHost *host;
    cchar   *segments[2];
    char    chunk[2048], buf[4096];
    Ticks   started;
    int     status;

    host = startTestHost("{ web: { documents: './site', listen: ['http://:4861'], limits: { upload: '1K' }, "
                         "routes: [ { match: '/upload/', methods: ['DELETE', 'GET', 'PUT'] }, {} ] } }");
    if (!host) {
        return;
    }
    started = rGetTicks();
    segments[0] = "PUT /upload/over.txt HTTP/1.1\r\nHost: x\r\nContent-Length: 4096\r\n\r\n";
    status = rawRequest(SMALL, segments, 1, 0, 1, buf, sizeof(buf));
    teqi(status, 413, "A PUT declaring more than the limit is refused on its headers");
    ttrue(rGetTicks() - started < 2 * TPS, "The refusal must not wait for the body");
    ttrue(!rFileExists("site/upload/over.txt"), "No file may be created");

    //  Chunked: 3 chunks of 512 bytes cross a 1K limit inside the handler
    SFMT(chunk, "200\r\n%0512d\r\n200\r\n%0512d\r\n200\r\n%0512d\r\n0\r\n\r\n", 0, 0, 0);
    segments[0] = "PUT /upload/chunked.txt HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n";
    segments[1] = chunk;
    status = rawRequest(SMALL, segments, 2, 0, 1, buf, sizeof(buf));
    teqi(status, 413, "A chunked PUT past the limit is refused as Payload Too Large");
    ttrue(!rFileExists("site/upload/chunked.txt"), "The partial file must be removed");

    //  Within the limit the same route works
    teqi(fetch(SMALL, "PUT", "/upload/small.txt", "hello", 5), 201);
    teqi(fetch(SMALL, "GET", "/upload/small.txt", NULL, 0), 200);
    teqi(fetch(SMALL, "DELETE", "/upload/small.txt", NULL, 0), 204);
    stopTestHost(host);
}

/*
    A client that disconnects mid-body leaves no file, and the server goes on serving
 */
static void testPutClientDisconnect(void)
{
    RSocket *sp;
    cchar   *req = "PUT /upload/partial.txt HTTP/1.1\r\nHost: x\r\nContent-Length: 100\r\n\r\n0123456789";
    int     status;

    unlink("site/upload/partial.txt");
    if ((sp = rawConnect(HTTP)) == 0) {
        tfail("Cannot connect");
        return;
    }
    rWriteSocket(sp, req, slen(req), rGetTicks() + 5 * TPS);
    rSleep(100);
    rFreeSocket(sp);
    rSleep(200);

    status = fetch(HTTP, "GET", "/upload/partial.txt", NULL, 0);
    teqi(status, 404, "A PUT cut off by the client must leave no file");
    status = fetch(HTTP, "GET", "/index.html", NULL, 0);
    teqi(status, 200, "The server keeps serving after the disconnect");
}

/*
    DELETE of a missing document and of a directory are refused and change nothing
 */
static void testDeleteRefusals(void)
{
    int status;

    status = fetch(HTTP, "DELETE", "/upload/does-not-exist.txt", NULL, 0);
    teqi(status, 404, "Deleting a missing document is refused");

    mkdir("site/upload/adir", 0755);
    status = fetch(HTTP, "DELETE", "/upload/adir", NULL, 0);
    teqi(status, 404, "Deleting a directory is refused");
    ttrue(rFileExists("site/upload/adir"), "The directory must still exist");
    rmdir("site/upload/adir");
}

/*
    A method the file handler does not implement is refused, even where the route permits it
 */
static void testUnsupportedMethodOnFile(void)
{
    cchar *segments[1];
    char  buf[4096];
    int   status;

    segments[0] = "TRACE /trace/index.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n";
    status = rawRequest(HTTP, segments, 1, 0, 1, buf, sizeof(buf));
    teqi(status, 405, "TRACE on a document is refused by the file handler");
}

/*
    The document root and request path are joined into a fixed-size server buffer. Every method
    must reject a path that does not fit before the partial join can name some other object.
 */
static void testLongDocumentPathRefused(void)
{
    Url    *up;
    char   *path, *url;
    size_t len;
    int    status;

    len = ME_MAX_FNAME + 64;
    path = rAlloc(len + 1);
    scopy(path, len + 1, "/upload/");
    memset(&path[slen(path)], 'a', len - slen(path));
    path[len] = '\0';
    url = sjoin(HTTP, path, NULL);
    up = urlAlloc(0);

    status = urlFetch(up, "GET", url, NULL, 0, NULL);
    teqi(status, 414, "An overlong GET path must be refused before file lookup");
    urlClose(up);
    status = urlFetch(up, "PUT", url, "x", 1, NULL);
    teqi(status, 414, "An overlong PUT path must be refused before file creation");
    urlClose(up);
    status = urlFetch(up, "DELETE", url, NULL, 0, NULL);
    teqi(status, 414, "An overlong DELETE path must be refused before file deletion");

    urlFree(up);
    rFree(url);
    rFree(path);
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
#if ME_WEB_MUTATIONS
        testPutOverLimit();
#else
        tinfo("Skipped: PUT and DELETE are unavailable on this platform");
#endif
        testPutClientDisconnect();
#if ME_WEB_MUTATIONS
        testDeleteRefusals();
#endif
        testUnsupportedMethodOnFile();
#if ME_WEB_MUTATIONS
        testLongDocumentPathRefused();
#endif
    }
    rFree(HTTP);
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
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
