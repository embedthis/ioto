/*
    containment.tst.c - Document roots and mutations must stay inside the document root.
 */
#include "test.h"

static char *HTTPS;

static int request(cchar *method, cchar *uri, cchar *body)
{
    Url *up;
    char url[1024];
    int status;

    up = urlAlloc(0);
    status = urlFetch(up, method, SFMT(url, "%s%s", HTTPS, uri), body, body ? slen(body) : 0, NULL);
    urlFree(up);
    return status;
}

static void expectRequest(cchar *method, cchar *uri, cchar *body, int expected)
{
    int status;

    status = request(method, uri, body);
    teqi(status, expected);
}

static void testRoots(void)
{
    cchar *roots[] = {"", "/", ".test/no-such-doc-root", "site/index.html", NULL};
    Json *config;
    WebHost *host;
    int i;

    for (i = 0; roots[i]; i++) {
        config = jsonAlloc();
        jsonSet(config, 0, "web.documents", roots[i], 0);
        host = webAllocHost(config, 0);
        tnull(host, "Invalid document root must fail allocation");
        if (host) {
            webFreeHost(host);
        }
        jsonFree(config);
    }
}

#if ME_UNIX_LIKE
static void testLinks(void)
{
    char dir[256], link[256], target[256], uri[256], created[256], inside[256];
    char *body, *absolute;
    cchar *methods[] = {"GET", "PUT", "DELETE", NULL};
    int i;

    SFMT(dir, "site-evil-%d", getpid());
    teqi(mkdir(dir, 0700), 0);
    SFMT(target, "%s/secret", dir);
    teqll(rWriteFile(target, "original", 8, 0600), 8);
    absolute = rGetRealPath(target);
    SFMT(link, "site/upload/escape-%d", getpid());
    SFMT(uri, "/upload/escape-%d", getpid());
    teqi(symlink(absolute, link), 0);
    rFree(absolute);
    for (i = 0; methods[i]; i++) {
        expectRequest(methods[i], uri, smatch(methods[i], "PUT") ? "changed" : NULL, 404);
    }
    body = rReadFile(target, NULL);
    tmatch(body, "original");
    rFree(body);
    unlink(link);

    absolute = rGetRealPath(dir);
    teqi(symlink(absolute, link), 0);
    rFree(absolute);
    SFMT(uri, "/upload/escape-%d/new", getpid());
    expectRequest("PUT", uri, "changed", 404);
    SFMT(created, "%s/new", dir);
    teqi(access(created, F_OK), -1, "Rejected PUT must not create an outside file");
    unlink(created);
    SFMT(uri, "/upload/escape-%d/secret", getpid());
    for (i = 0; methods[i]; i++) {
        expectRequest(methods[i], uri, smatch(methods[i], "PUT") ? "changed" : NULL, 404);
    }
    body = rReadFile(target, NULL);
    tmatch(body, "original");
    rFree(body);
    unlink(link);
    unlink(target);
    rmdir(dir);

    SFMT(inside, "site/upload/inside-%d", getpid());
    teqll(rWriteFile(inside, "inside", 6, 0600), 6);
    absolute = rGetRealPath(inside);
    teqi(symlink(absolute, link), 0);
    rFree(absolute);
    SFMT(uri, "/upload/escape-%d", getpid());
    for (i = 0; methods[i]; i++) {
        expectRequest(methods[i], uri, smatch(methods[i], "PUT") ? "changed" : NULL, 404);
    }
    body = rReadFile(inside, NULL);
    tmatch(body, "inside");
    rFree(body);
    unlink(link);
    unlink(inside);

    SFMT(uri, "/upload/normal-%d", getpid());
    expectRequest("PUT", uri, "first", 201);
    expectRequest("PUT", uri, "second", 204);
    expectRequest("GET", uri, NULL, 200);
    expectRequest("DELETE", uri, NULL, 204);
}
#endif

/* A streamed over-limit replacement must leave the previous document intact. */
static void testFailedPutPreservesFile(void)
{
    WebHost *host;
    char path[256], header[512], response[4096], *body;
    cchar *segments[4];
    int status;

    host = startTestHost("{web: {documents: './site', listen: ['http://:18524'], "
                         "limits: {upload: '4'}, routes: [{match: '/upload/', methods: ['PUT']}]}}");
    if (!host) {
        return;
    }
    SFMT(path, "site/upload/preserve-%d", getpid());
    teqll(rWriteFile(path, "original", 8, 0600), 8);
    SFMT(header, "PUT /upload/preserve-%d HTTP/1.1\r\nHost: x\r\n"
         "Transfer-Encoding: chunked\r\nConnection: close\r\n\r\n", getpid());
    segments[0] = header;
    segments[1] = "3\r\nabc\r\n";
    segments[2] = "3\r\ndef\r\n";
    segments[3] = "0\r\n\r\n";
    status = rawRequest("http://localhost:18524", segments, 4, 0, 1, response, sizeof(response));
    teqi(status, 413);
    body = rReadFile(path, NULL);
    tmatch(body, "original", "Failed replacement must retain prior contents");
    rFree(body);
    unlink(path);
    stopTestHost(host);
}

static void fiberMain(void *arg)
{
    webInit();
    testRoots();
#if ME_WEB_MUTATIONS
    testFailedPutPreservesFile();
#else
    tinfo("Skipped: PUT and DELETE are unavailable on this platform");
#endif
    if (setup(NULL, &HTTPS)) {
#if ME_UNIX_LIKE
        testLinks();
#endif
    }
    rFree(HTTPS);
    webTerm();
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
    return 0;
}
