

/*
    buffer.c.tst - Tests for output buffering

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void testBuffer(void)
{
    Url   *up;
    cchar *contentLength, *response;
    char  url[128];
    int   status;

    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/test/buffer", HTTP), NULL, 0, NULL);
    ttrue(status == 200);
    response = urlGetResponse(up);

    /*
        Expect buffered output to have a content length and not be transfer encoded.
     */
    contentLength = urlGetHeader(up, "Content-Length");
    ttrue(stoi(contentLength) > 0);

    ttrue(scontains(response, "Hello World 1"));
    ttrue(scontains(response, "Hello World 7"));
    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testBuffer();
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
