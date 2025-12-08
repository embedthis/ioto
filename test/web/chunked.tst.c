

/*
    chunked.c.tst - Unit tests for chunked responses

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void getChunked(int count)
{
    Url   *up;
    cchar *response;
    char  body[16], url[128], pattern[64];
    int   status;

    /*
        bulk response is always transfer encoded
     */
    up = urlAlloc(0);
    SFMT(body, "count=%d", count);
    status = urlFetch(up, "GET", SFMT(url, "%s/test/bulk", HTTP), body, (size_t) -1,
                      "Content-Type: application/x-www-form-urlencoded\r\n");
    ttrue(status == 200);
    response = urlGetResponse(up);
    tcontains(response, "Hello World 00000000");
    SFMT(pattern, "%010d\n", count - 1);
    ttrue(sends(response, pattern));
    urlFree(up);
}

static void testChunked(void)
{
    getChunked(1);
    getChunked(100);
    getChunked(1000);
    getChunked(10000);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testChunked();
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
