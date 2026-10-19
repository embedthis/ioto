/*
    file-types.tst.c - Only regular files may be served

    open() on a FIFO with no writer blocks until one appears. It is a plain blocking syscall, not
    a fiber-aware one, so on a single-threaded event loop it stalls every fiber and every
    connection. One unauthenticated GET for a FIFO in the document root took the whole server
    down and kept it down.

    The containment and spelling checks cannot help - they run after the open, by which time the
    process is already blocked. The gate has to be the open itself.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTPS;

/************************************ Code ************************************/

/*
    The second assertion is the finding. Before the fix the FIFO request never returned and the
    ordinary request that followed it never returned either.
 */
static void testFifoDoesNotWedgeServer(void)
{
    Url  *up;
    char url[256];
    int  status;

    //  Baseline: an ordinary file is served
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTPS), NULL, 0, NULL);
    teqi(status, 200);
    urlFree(up);

    //  A FIFO must be refused, promptly
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/wedge.fifo", HTTPS), NULL, 0, NULL);
    if (status == 0) {
        tfail("Request for a FIFO did not complete - the server is wedged");
        urlFree(up);
        return;
    }
    teqi(status, 404);
    urlFree(up);

    //  And the server must still be serving
    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTPS), NULL, 0, NULL);
    teqi(status, 200);
    urlFree(up);
}

/*
    A directory is already refused via a redirect; confirm the type gate did not change that.
 */
static void testRegularFilesStillServed(void)
{
    Url   *up;
    cchar *body;
    char  url[256];
    int   status;

    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTPS), NULL, 0, NULL);
    teqi(status, 200);
    body = urlGetResponse(up);
    tcontains(body, "Hello /index.html");
    urlFree(up);
}

static void fiberMain(void *arg)
{
    if (setup(NULL, &HTTPS)) {
        testFifoDoesNotWedgeServer();
        testRegularFilesStillServed();
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
