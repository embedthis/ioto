/*
    parse.c.tst - Unit tests for Parsing

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static cchar    *HTTP;
static cchar    *HTTPS;

/************************************ Code ************************************/

static void parseUrl()
{
    Url     *up;  
    int     rc;

    up = urlAlloc(0);
    rc = urlParse(up, NULL);
    ttrue(rc < 0);

    rc = urlParse(up, "");
    ttrue(rc == 0);
    tmatch(up->scheme, "http");
    tmatch(up->host, "localhost");
    ttrue(up->port == 80);
    tmatch(up->path, "");
    ttrue(up->query == 0);
    ttrue(up->hash == 0);

    rc = urlParse(up, "http://www.example.com:1234/index.html?query=true#frag");
    ttrue(rc == 0);
    tmatch(up->scheme, "http");
    tmatch(up->host, "www.example.com");
    ttrue(up->port == 1234);
    tmatch(up->path, "index.html");
    tmatch(up->query, "query=true");
    tmatch(up->hash, "frag");
    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        parseUrl();
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
