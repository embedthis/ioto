/*
    deny-files.tst.c - Deny-by-default filename policy

    The file handler served any regular file under the document root that a route matched, and a
    route matches on the path prefix, so a filename policy could not be expressed at all. Anything
    that landed in the document tree - by a build step, a deployment script, an editor or a git
    clone - was reachable: ".env", "*.bak", editor backups, source maps, and a ".git" directory
    whose object store yields every secret ever committed.

    The fixtures come from prep.sh. The dot-directory case is ".hidden" rather than ".git": a real
    ".git" under the document root would make git treat site/ as a nested repository. The pattern
    that refuses it, ".*", is the same one that refuses ".git".

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

/************************************ Code ************************************/

static int fetch(cchar *method, cchar *path, cchar *headers)
{
    Url  *up;
    char url[256];
    int  status;

    up = urlAlloc(0);
    status = urlFetch(up, method, SFMT(url, "%s%s", HTTP, path), NULL, 0, headers);
    urlFree(up);
    return status;
}

/*
    A denied file is 404, never 403: a 403 confirms the file is there, which for "/.git/config" is
    most of what the attacker wanted.
 */
static void testDeniedNamesAreNotServed(void)
{
    teqi(fetch("GET", "/.env", NULL), 404, "A dot-file must not be served");
    teqi(fetch("GET", "/x.bak", NULL), 404, "A backup file must not be served");
    teqi(fetch("GET", "/index.html~", NULL), 404, "An editor backup must not be served");
    teqi(fetch("GET", "/app.js.map", NULL), 404, "A source map must not be served");
}

/*
    Every segment is tested, not only the last, so the whole of a dot-directory is refused rather
    than just its own name. This is the case that matters: "/.git" alone being refused while
    "/.git/objects/ab/cdef" is served closes nothing.
 */
static void testDeniedDirectoryIsRefusedThroughout(void)
{
    teqi(fetch("GET", "/.hidden", NULL), 404, "A dot-directory must not be served");
    teqi(fetch("GET", "/.hidden/secret.txt", NULL), 404, "A file inside a dot-directory must not be served");
    teqi(fetch("GET", "/.hidden/nothing/here.txt", NULL), 404, "A deep path under a dot-directory must not be served");
}

/*
    The policy runs ahead of pickFile, which appends ".gz" and ".br", so the pre-compressed lookup
    cannot be used to reach a denied name.
 */
static void testPreCompressedLookupCannotReachDeniedFile(void)
{
    teqi(fetch("GET", "/compressed/.env", "Accept-Encoding: gzip, br\r\n"), 404,
         "A denied name is refused before the compressed variants are tried");
}

/*
    The check is in webFileHandler rather than getFile, so it covers the writing methods too. The
    /upload/ route is the one configured for PUT and DELETE.
 */
static void testWritingMethodsAreDeniedToo(void)
{
    teqi(fetch("PUT", "/upload/.evil", NULL), 404, "PUT to a denied name must be refused");
    teqi(fetch("DELETE", "/upload/.evil", NULL), 404, "DELETE of a denied name must be refused");
}

/*
    Ordinary documents are untouched. Without this the tests above would pass against a handler
    that refused everything.
 */
static void testOrdinaryDocumentsStillServed(void)
{
    teqi(fetch("GET", "/index.html", NULL), 200, "An ordinary document is unaffected");
    teqi(fetch("GET", "/dir/index.html", NULL), 200, "An ordinary nested document is unaffected");
    teqi(fetch("GET", "/x.secret", NULL), 200, "A name outside the default list is unaffected");
}

/*
    Configuring web.deny replaces the built-in list rather than adding to it, so a site that
    genuinely serves a dot-directory can do so - but has to say so.
 */
static void testConfiguredDenyReplacesTheDefault(void)
{
    WebHost *host;
    Url     *up;
    char    url[256];
    int     status;
    cchar   *config =
        "{"
        "    web: {"
        "        listen: [ 'http://127.0.0.1:4874' ],"
        "        documents: './site',"
        "        index: 'index.html',"
        "        routes: [ {} ],"
        "        deny: [ '*.secret' ],"
        "    }"
        "}";

    if ((host = startTestHost(config)) == 0) {
        return;
    }
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "http://127.0.0.1:4874/x.secret"), NULL, 0, NULL);
    teqi(status, 404, "The configured pattern is enforced");
    urlFree(up);

    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "http://127.0.0.1:4874/.env"), NULL, 0, NULL);
    teqi(status, 200, "The built-in list is replaced, not extended, by a configured deny");
    urlFree(up);

    stopTestHost(host);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, NULL)) {
        testDeniedNamesAreNotServed();
        testDeniedDirectoryIsRefusedThroughout();
        testPreCompressedLookupCannotReachDeniedFile();
        testWritingMethodsAreDeniedToo();
        testOrdinaryDocumentsStillServed();
        testConfiguredDenyReplacesTheDefault();
    }
    rFree(HTTP);
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
