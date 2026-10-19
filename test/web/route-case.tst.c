/*
    route-case.tst.c - The route gate and the file open must agree about path identity

    Route selection is the only authorization gate in the server and it compared the request path
    against route->match byte for byte. A case-folding file system - APFS, HFS+, NTFS, vfat, exfat -
    resolves many spellings to one file, so "/Admin/secret.html" missed the {match: '/admin/',
    role: 'admin'} route, fell through to the catch-all route that carries no role, and the file
    handler was asked for the very file the admin route exists to protect.

    Where the on-disk directory is itself spelled differently from the route - documents/Restricted
    guarded by {match: '/restricted/'} - the file handler's spelling check agrees with the request
    and the protected body was served, unauthenticated, with a 200.

    These tests only mean anything where the documents file system folds case, so they probe it and
    skip rather than pass vacuously on ext4.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

/************************************ Code ************************************/

/*
    Does the documents file system fold case? Ask it, rather than assume from the platform: the
    suite runs on ext4, APFS and NTFS, and a vfat or SMB checkout on Linux folds case too.
 */
static bool documentsFoldCase(void)
{
    struct stat info, altInfo;

    if (stat("site/index.html", &info) < 0 || stat("site/INDEX.HTML", &altInfo) < 0) {
        return 0;
    }
    return info.st_ino == altInfo.st_ino && info.st_dev == altInfo.st_dev;
}

static int fetchStatus(cchar *uri)
{
    Url  *up;
    char url[512];
    int  status;

    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s%s", HTTP, uri), NULL, 0, NULL);
    urlFree(up);
    return status;
}

static int fetchStatusAsAdmin(cchar *uri)
{
    Url  *up;
    char url[512];
    int  status;

    up = urlAlloc(0);
    urlSetAuth(up, "alice", "password", "digest");
    status = urlFetch(up, "GET", SFMT(url, "%s%s", HTTP, uri), NULL, 0, NULL);
    urlFree(up);
    return status;
}

static char *fetchBody(cchar *uri, int *status)
{
    Url  *up;
    char url[512];
    char *body;

    up = urlAlloc(0);
    *status = urlFetch(up, "GET", SFMT(url, "%s%s", HTTP, uri), NULL, 0, NULL);
    body = sclone(urlGetResponse(up));
    urlFree(up);
    return body;
}

/*
    Every spelling the file system treats as the protected path must reach the protected route and
    be challenged. A 404 from the file handler is not good enough: it means the gate was missed and
    the request was refused for an unrelated reason, which stops protecting the file the moment the
    on-disk name is spelled differently from the route.
 */
static void testCaseVariantsReachTheRouteGate(void)
{
    cchar *variants[] = {
        "/ADMIN/secret.html",
        "/Admin/secret.html",
        "/aDmIn/secret.html",
        "/admin/SECRET.HTML",
        NULL,
    };
    int i, status;

    for (i = 0; variants[i]; i++) {
        status = fetchStatus(variants[i]);
        teqi(status, 401, "A case variant of a protected path must reach the route gate: %s", variants[i]);
    }
}

/*
    The bypass with teeth. site/Restricted is guarded by {match: '/restricted/', role: 'admin'},
    so the route spelling and the directory spelling differ. Before the fix the capitalised request
    missed the route, the file handler resolved it to the file it asked for, and the body was
    served with a 200.
 */
static void testMixedCaseDocumentDirectoryIsProtected(void)
{
    char *body;
    bool leaked;
    int  status;

    body = fetchBody("/Restricted/secret.html", &status);
    teqi(status, 401, "The on-disk spelling of a protected directory must be challenged");
    leaked = scontains(body, "restricted-body") != 0;
    tfalse(leaked, "The protected body must not be served");
    rFree(body);

    //  The route's own spelling resolves to the same file and must be challenged too
    status = fetchStatus("/restricted/secret.html");
    teqi(status, 401, "The route's own spelling must still be challenged");
}

/*
    Folding the gate must not deny the authorized user, and must not disturb exact spellings.
 */
static void testAuthorizedAccessUnaffected(void)
{
    int status;

    status = fetchStatusAsAdmin("/admin/secret.html");
    teqi(status, 200, "An authenticated admin must still be served");

    status = fetchStatusAsAdmin("/Restricted/secret.html");
    teqi(status, 200, "An authenticated admin must be served the on-disk spelling");

    status = fetchStatus("/index.html");
    teqi(status, 200, "A public document must still be served");

    status = fetchStatus("/static/index.html");
    teqi(status, 200, "A public document under a prefix route must still be served");
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        if (documentsFoldCase()) {
            testCaseVariantsReachTheRouteGate();
            testMixedCaseDocumentDirectoryIsProtected();
            testAuthorizedAccessUnaffected();
        } else {
            tskip("Documents file system is case-sensitive: the route gate and open() cannot diverge\n");
        }
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
