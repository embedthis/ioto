/*
    get.c.tst - Unit tests for get requests

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char    *HTTP;
static char    *HTTPS;

/************************************ Code ************************************/

static void get()
{
    Url     *up;
    cchar   *response;
    char    url[128];
    int     status;

    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    teq(200, status);
    response = urlGetResponse(up);
    tcontains(response, "Hello /index.html");
    ttrue(sstarts(response, "<html>"));
    print("RESPONSE >>>>\n%s\n", response);
    ttrue(scontains(response, "</html>"));
    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        get();
    }
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
