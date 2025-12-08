

/*
    dir.c.tst - Unit tests for get directory / index

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void getDir()
{
    Url   *up;
    cchar *response;
    char  url[128];
    int   status;

    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/", HTTP), NULL, 0, NULL);
    ttrue(status == 200);
    response = urlGetResponse(up);
    tcontains(response, "Hello /index.html");
    ttrue(sstarts(response, "<html>"));
    tcontains(response, "</html>");
    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        getDir();
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
