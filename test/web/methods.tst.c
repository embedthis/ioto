

/*
    methods.c.tst - Unit tests for HTTP methods

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void checkMethods()
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // standard get
    status = urlFetch(up, "GET", SFMT(url, "%s/test/success", HTTP), NULL, 0, NULL);
    ttrue(status == 200);

    // Failing get
    status = urlFetch(up, "GET", SFMT(url, "%s/UNKNOWN.FILE", HTTP), NULL, 0, NULL);
    ttrue(status == 404);

    // methods are caseless
    status = urlFetch(up, "Get", SFMT(url, "%s/test/success", HTTP), NULL, 0, NULL);
    ttrue(status == 200);

    // Head
    status = urlFetch(up, "HEAD", SFMT(url, "%s/trace/index.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200);
    ttrue(stoi(urlGetHeader(up, "Content-Length")) > 0);
    ttrue(up->rxLen > 0);
    ttrue(up->rxRemaining == 0);
    tmatch(urlGetResponse(up), "");

    // Post
    status = urlFetch(up, "POST", SFMT(url, "%s/test/success", HTTP), NULL, 0, NULL);
    ttrue(status == 200);

    // PUT
    status = urlFetch(up, "PUT", SFMT(url, "%s/upload/temp.dat", HTTP), NULL, 0, NULL);
    ttrue(status == 201 || status == 204);

    // Delete
    status = urlFetch(up, "DELETE", SFMT(url, "%s/upload/temp.dat", HTTP), NULL, 0, NULL);
    ttrue(status == 204);

    // Delete unknown file
    status = urlFetch(up, "DELETE", SFMT(url, "%s/upload/UNKNOWN.FILE", HTTP), NULL, 0, NULL);
    ttrue(status == 404);

    //  Options
    status = urlFetch(up, "OPTIONS", SFMT(url, "%s/trace/index.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200);
    tmatch(urlGetHeader(up, "Access-Control-Allow-Methods"), "DELETE,GET,HEAD,OPTIONS,POST,PUT,TRACE");

    //  Trace should be disabled by default
    status = urlFetch(up, "TRACE", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    ttrue(status == 405);

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        checkMethods();
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
