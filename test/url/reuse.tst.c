/*
    reuse.tst.c - Test connection reuse when host/port changes

    Copyright (c) All Rights Reserved. See details at the end of the file.
*/

/********************************** Includes **********************************/

#include "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

/*
    Test that same host/port reuses the connection (socket pointer unchanged).
    This verifies the core fix: prior host/port are saved BEFORE parsing,
    so the comparison correctly detects when values haven't changed.
*/
static void testSameHostReuse(void)
{
    Url     *up;
    RSocket *sock1, *sock2;
    char    url[128];

    up = urlAlloc(0);

    // First request
    urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), 0, 0, 0);
    ttrue(up->status == 200);
    sock1 = up->sock;

    // Second request to same host/port - should reuse
    urlFetch(up, "GET", SFMT(url, "%s/size/1K.txt", HTTP), 0, 0, 0);
    ttrue(up->status == 200);
    sock2 = up->sock;

    // Should reuse the same socket object (not freed and reallocated)
    ttrue(sock1 == sock2);

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testSameHostReuse();
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
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
*/
