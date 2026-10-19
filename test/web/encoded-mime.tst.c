/*
    encoded-mime.tst.c - MIME and cache policy follow the decoded path (issue 21)
 */

#include "test.h"

static char *HTTP;

static void checkResponse(cchar *path, cchar *type, bool cached)
{
    cchar *segments[1];
    char  request[1024], response[16384], expected[128], *end;
    int   status;

    SFMT(request, "GET %s HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", path);
    segments[0] = request;
    status = rawRequest(HTTP, segments, 1, 0, 1, response, sizeof(response));
    teqi(status, 200, "%s", path);
    end = scontains(response, "\r\n\r\n");
    tnotnull(end);
    if (end) {
        end[2] = '\0';
    }
    SFMT(expected, "Content-Type: %s\r\n", type);
    tcontains(response, expected, "%s", path);
    end = scontains(response, "Content-Type:");
    if (end) {
        tnull(scontains(end + 1, "Content-Type:"), "Content-Type must appear once");
    }
    tcontains(response, "X-Content-Type-Options: nosniff\r\n", "%s", path);
    if (cached) {
        tcontains(response, "Cache-Control:", "%s", path);
        tcontains(response, "max-age=", "%s", path);
        tcontains(response, "public", "%s", path);
    }
}

static void testDecodedMimeAndCache(void)
{
    checkResponse("/index.html", "text/html", 0);
    checkResponse("/index%2Ehtml", "text/html", 0);
    checkResponse("/%69ndex.html", "text/html", 0);
    checkResponse("/index.%68tml", "text/html", 0);
    checkResponse("/gone.css/../index%2Ehtml?a=b#c", "text/html", 0);
    checkResponse("/", "text/html", 0);
    checkResponse("/trace/index%2Ehtml", "text/html", 0);
    checkResponse("/test/show", "application/json", 0);
    checkResponse("/static/style.css", "text/css", 1);
    checkResponse("/static/style%2Ecss", "text/css", 1);
    checkResponse("/st%61tic/style.css", "text/css", 1);
}

static void testUnknownMime(void)
{
    char path[128], file[128];
    int  rc;

    //  A chunked action response without an explicit type also receives the fallback.
    checkResponse("/test/stream", "application/octet-stream", 0);

    SFMT(path, "/upload/mime-%d.unknown", getpid());
    SFMT(file, "site%s", path);
    rc = (int) rWriteFile(file, "<h1>untyped</h1>", 15, 0644);
    teqi(rc, 15);
    checkResponse(path, "application/octet-stream", 0);
    unlink(file);

    SFMT(path, "/upload/mime-%d", getpid());
    SFMT(file, "site%s", path);
    rc = (int) rWriteFile(file, "untyped", 7, 0644);
    teqi(rc, 7);
    checkResponse(path, "application/octet-stream", 0);
    unlink(file);
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        testDecodedMimeAndCache();
        testUnknownMime();
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
