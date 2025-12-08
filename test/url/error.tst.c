/*
    error.c.tst - Unit tests for error

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char    *HTTP;
static char    *HTTPS;

/************************************ Code ************************************/

static void errorUrl()
{
    Url    *up;
    int    status;

    up = urlAlloc(0);
    status = urlFetch(up, "GET", "https://UNKNOWN-1237811.com/", 0, 0, 0);
    ttrue(status < 404);
    ttrue(scontains(urlGetError(up), "Cannot find address of UNKNOWN"));
    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        errorUrl();
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
