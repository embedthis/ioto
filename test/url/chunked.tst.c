/*
    chunked.c.tst - Unit tests for chunked post requests

    Needs dedicated appweb server

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char    *HTTP;
static char    *HTTPS;

/************************************ Code ************************************/

static void chunkedPostUrl()
{
    Url    *up;
    char   url[128];
    Json   *json;

    up = urlAlloc(0);

    if (urlStart(up, "POST", SFMT(url, "%s/test/show", HTTP)) < 0) {
        tfail("Cannot start request");
    } else if (urlWrite(up, "Hello", 0) < 0 || urlWrite(up, " World", 0) < 0 || urlFinalize(up) < 0) {
        tfail("Cannot write");
    } else if (urlGetResponse(up) == 0) {
        tfail("Cannot get response");
    } else {
        ttrue(up->status == 200);
        json = urlGetJsonResponse(up);
        tmatch(jsonGet(json, 0, "body", 0), "Hello World");
        jsonFree(json);
    }
    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        chunkedPostUrl();
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
