/*
    get.c.tst - Unit tests for get requests

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static cchar    *HTTP;
static cchar    *HTTPS;

/************************************ Code ************************************/

static void getUrl()
{
    char    *response, url[128];

    response = urlGet(SFMT(url, "%s/index.html", HTTP), 0);
    ttrue(response);
    ttrue(scontains(response, "Hello /index.html"));
    rFree(response);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        getUrl();
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
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
