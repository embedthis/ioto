

/*
    headers.c.tst - Unit tests for response headers

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void checkResponseHeaders()
{
    Url   *up;
    cchar *response;
    char  url[128];
    int   status;

    up = urlAlloc(0);

    status = urlFetch(up, "GET", SFMT(url, "%s/test/success", HTTP), NULL, 0, NULL);
    ttrue(status == 200);
    response = urlGetResponse(up);
    tmatch(response, "success\n");

    //  Check expected headers
    tmatch(urlGetHeader(up, "Content-Type"), "text/plain");
    tmatch(urlGetHeader(up, "Content-Length"), "8");
    tmatch(urlGetHeader(up, "Connection"), "keep-alive");

    //  Check case insensitive
    tmatch(urlGetHeader(up, "connection"), "keep-alive");
    tmatch(urlGetHeader(up, "content-length"), "8");

    //  Check unexpected headers
    ttrue(!urlGetHeader(up, "Last-Modified"));
    ttrue(!urlGetHeader(up, "ETag"));


    //  Static file
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200);
    //  Check expected headers
    ttrue(urlGetHeader(up, "Last-Modified"));
    ttrue(urlGetHeader(up, "ETag"));

    urlFree(up);
}

static void setHeaders()
{
    Json *json;
    char url[128];

    json = urlGetJson(SFMT(url, "%s/test/show", HTTP), "X-TEST: 42\r\n");
    tmatch(jsonGet(json, 0, "headers['X-TEST']", 0), "42");
    jsonFree(json);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        checkResponseHeaders();
        setHeaders();
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
