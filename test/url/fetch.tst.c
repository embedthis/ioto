/*
    fetch.c.tst - Unit tests for fetch

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char    *HTTP;
static char    *HTTPS;

/************************************ Code ************************************/

static void fetchUrl()
{
    Url     *up;
    Json    *json;
    char    url[128];
    int     status;

    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/test/show", HTTP), 0, 0, 0);
    ttrue(status == 200);
    json = urlGetJsonResponse(up);
    tmatch(jsonGet(json, 0, "url", 0), "/test/show");
    tmatch(jsonGet(json, 0, "method", 0), "GET");
    jsonFree(json);
    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        fetchUrl();
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
