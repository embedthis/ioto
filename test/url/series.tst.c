/*
    series.c.tst - Unit tests for requests in series

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char    *HTTP;
static char    *HTTPS;

/************************************ Code ************************************/

static void seriesUrl()
{
    Url     *up;
    char    url[128];
    cchar   *response;
    int     status;

    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), 0, 0, 0);
    ttrue(status == 200);
    response = urlGetResponse(up);
    ttrue(scontains(urlGetHeader(up, "Connection"), "keep-alive"));
    urlClose(up);

    //  Should use keep alive to reuse same socket
    status = urlFetch(up, "GET", SFMT(url, "%s/size/1K.txt", HTTP), 0, 0, 0);
    ttrue(status == 200);
    response = urlGetResponse(up);
    ttrue(scontains(response, "END OF DOCUMENT"));

    //  Force a new socket
    urlClose(up);
    status = urlFetch(up, "GET", SFMT(url, "%s/size/10K.txt", HTTP), 0, 0, 0);
    ttrue(status == 200);
    response = urlGetResponse(up);
    ttrue(scontains(response, "END OF DOCUMENT"));

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        seriesUrl();
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
