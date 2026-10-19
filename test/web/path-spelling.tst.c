/*
    path-spelling.tst.c - A URL must reach a file only by that file's own name

    Route selection is the only authorization gate in the server and it matches the request path
    byte for byte. The file system does not: APFS, HFS+, NTFS, vfat and exfat fold case, and
    APFS and HFS+ fold Unicode normalization form. A request spelled differently from a protected
    route therefore misses that route, falls through to a later unauthenticated route, and the
    file system resolves it to the protected file anyway.

    Before the fix, /admin/secret.html returned 401 and /ADMIN/secret.html returned 200 with the
    protected body.

    These tests run over TLS. The plaintext path uses sendfile(2), which some sandboxed build
    environments deny; the TLS path does not, so the assertions here are about the server rather
    than about the host's syscall policy.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTPS;

/************************************ Code ************************************/

static int fetchStatus(cchar *uri)
{
    Url  *up;
    char url[512];
    int  status;

    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s%s", HTTPS, uri), NULL, 0, NULL);
    urlFree(up);
    return status;
}

static char *fetchBody(cchar *uri, int *status)
{
    Url   *up;
    char  url[512];
    char  *body;

    up = urlAlloc(0);
    *status = urlFetch(up, "GET", SFMT(url, "%s%s", HTTPS, uri), NULL, 0, NULL);
    body = sclone(urlGetResponse(up));
    urlFree(up);
    return body;
}

/*
    The exact spelling must still work. Without this the suite would pass by rejecting everything.
 */
static void testExactSpellingServed(void)
{
    char *body;
    int  status;

    body = fetchBody("/index.html", &status);
    teqi(status, 200);
    tcontains(body, "Hello /index.html");
    rFree(body);

    //  A directory request must still resolve through the configured index
    body = fetchBody("/", &status);
    teqi(status, 200);
    tcontains(body, "Hello /index.html");
    rFree(body);

    //  A nested exact path must still work
    teqi(fetchStatus("/static/index.html"), 200);
}

/*
    The protected route must still challenge on its own spelling. If this returned 404 the
    bypass tests below would pass for the wrong reason.
 */
static void testProtectedRouteStillChallenges(void)
{
    teqi(fetchStatus("/admin/secret.html"), 401);
}

/*
    The bypass itself. Every case permutation of a protected prefix must be refused. None may
    return 200, because 200 means the protected body was served without authentication.
 */
static void testCaseVariantsOfProtectedPathRefused(void)
{
    cchar *variants[] = {
        "/ADMIN/secret.html",
        "/Admin/secret.html",
        "/aDmIn/secret.html",
        "/ADMIN/SECRET.HTML",
        "/admin/SECRET.HTML",
        "/admin/Secret.html",
        NULL,
    };
    int i, status;

    for (i = 0; variants[i]; i++) {
        status = fetchStatus(variants[i]);
        //  401 (route matched, auth required) or 404 (refused) are both acceptable. 200 is not.
        tfalse(status == 200, "A case variant of a protected path must never be served");
        ttrue(status == 401 || status == 404, "A case variant must be challenged or refused");
    }
}

/*
    The same rule applies to public files. One spelling, one file.
 */
static void testCaseVariantsOfPublicPathRefused(void)
{
    int status;

    //  The on-disk name is MixedCase.html
    teqi(fetchStatus("/unicode/MixedCase.html"), 200);

    status = fetchStatus("/unicode/mixedcase.html");
    tfalse(status == 200, "A lowercased spelling must not resolve to MixedCase.html");

    status = fetchStatus("/unicode/MIXEDCASE.HTML");
    tfalse(status == 200, "An uppercased spelling must not resolve to MixedCase.html");
}

/*
    Unicode normalization is the second fold. The file is stored NFC; the NFD spelling opens the
    same file on APFS and HFS+, so the server must reject it on the resolved name.
    café.html is NFC "caf%C3%A9"; the NFD spelling is "cafe%CC%81".
 */
static void testUnicodeNormalizationVariantRefused(void)
{
    char *body;
    int  status;

    body = fetchBody("/unicode/caf%C3%A9.html", &status);
    if (status != 200) {
        tinfo("Skipped: Unicode fixture not present");
        rFree(body);
        return;
    }
    tcontains(body, "cafe-nfc");
    rFree(body);

    status = fetchStatus("/unicode/cafe%CC%81.html");
    tfalse(status == 200, "The NFD spelling must not resolve to the NFC file");
}

/*
    Traversal must not escape the document root. The containment half of the check.
 */
static void testTraversalRefused(void)
{
    cchar *variants[] = {
        "/../web.json5",
        "/../../etc/passwd",
        "/admin/../../web.json5",
        "/%2e%2e/web.json5",
        NULL,
    };
    int i, status;

    for (i = 0; variants[i]; i++) {
        status = fetchStatus(variants[i]);
        tfalse(status == 200, "A traversal outside the document root must never be served");
    }
}

/*
    A refusal must be a 404, never a redirect to the canonical spelling. A redirect confirms the
    resource exists, which is exactly what must not leak on a protected path.
 */
static void testRefusalDoesNotRedirect(void)
{
    Url   *up;
    char  url[512];
    int   status;

    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/ADMIN/secret.html", HTTPS), NULL, 0, NULL);
    tfalse(status == 301, "A refused spelling must not redirect to the canonical spelling");
    tfalse(status == 302, "A refused spelling must not redirect to the canonical spelling");
    tnull(urlGetHeader(up, "Location"));
    urlFree(up);
}

static void fiberMain(void *arg)
{
    if (setup(NULL, &HTTPS)) {
        testExactSpellingServed();
        testProtectedRouteStillChallenges();
        testCaseVariantsOfProtectedPathRefused();
        testCaseVariantsOfPublicPathRefused();
        testUnicodeNormalizationVariantRefused();
        testTraversalRefused();
        testRefusalDoesNotRedirect();
    }
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
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
