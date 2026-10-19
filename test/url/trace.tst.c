/*
    trace.tst.c - Unit tests for the request/response tracing flags

    The URL_SHOW flags and the URL_SHOW environment variable drive the diagnostic logging paths.
    Those paths run only when tracing is enabled, so nothing else in the suite reaches them.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char    *HTTP;
static char    *HTTPS;

/************************************ Code ************************************/

/*
    Explicit flags trace the request headers, request body, response headers and response body
 */
static void testShowFlags()
{
    Url  *up;
    char url[128];

    up = urlAlloc(URL_SHOW_REQ_HEADERS | URL_SHOW_REQ_BODY | URL_SHOW_RESP_HEADERS | URL_SHOW_RESP_BODY);
    ttrue(up->flags == (URL_SHOW_REQ_HEADERS | URL_SHOW_REQ_BODY | URL_SHOW_RESP_HEADERS | URL_SHOW_RESP_BODY));

    ttrue(urlFetch(up, "POST", SFMT(url, "%s/test/show", HTTP), "traced body", 0, NULL) == 200);
    tcontains(urlGetResponse(up), "traced body");

    //  urlClose traces the disconnect when request headers are shown
    urlClose(up);
    urlFree(up);
}

static void testShowBinaryBody()
{
    Url  *up;
    char body[64];
    char url[128];
    int  i;

    for (i = 0; i < (int) sizeof(body); i++) {
        body[i] = (char) i;
    }
    /*
        Stage the document directly. What is under test is how a binary response body is traced,
        and staging it with a PUT made that depend on the server's ability to create a document,
        which needs descriptor-relative operations not every platform provides.
     */
    ttrue(rWriteFile("site/binary.dat", body, sizeof(body), 0644) == sizeof(body));

    up = urlAlloc(URL_SHOW_RESP_BODY);
    ttrue(urlFetch(up, "GET", SFMT(url, "%s/binary.dat", HTTP), NULL, 0, NULL) == 200);
    ttrue(rGetBufLength(urlGetResponseBuf(up)) == sizeof(body));
    ttrue(memcmp(rGetBufStart(urlGetResponseBuf(up)), body, sizeof(body)) == 0);
    urlFree(up);

    unlink("site/binary.dat");
}

/*
    A printable response body is traced as text
 */
static void testShowTextBody()
{
    Url  *up;
    char url[128];

    up = urlAlloc(URL_SHOW_RESP_BODY);
    ttrue(urlFetch(up, "PUT", SFMT(url, "%s/test/put", HTTP), "printable", 0, NULL) == 200);
    tcontains(urlGetResponse(up), "9");
    urlFree(up);
}

/*
    URL_SHOW in the environment selects the same flags. H and B are the request, h and b the response.
 */
static void testShowEnvironment()
{
    Url  *up;
    char url[128];

    ttrue(setenv("URL_SHOW", "HBhb", 1) == 0);
    up = urlAlloc(0);
    ttrue(up->flags & URL_SHOW_REQ_HEADERS);
    ttrue(up->flags & URL_SHOW_REQ_BODY);
    ttrue(up->flags & URL_SHOW_RESP_HEADERS);
    ttrue(up->flags & URL_SHOW_RESP_BODY);
    ttrue(urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL) == 200);
    urlFree(up);

    //  An unrecognised setting selects URL_SHOW_NONE rather than leaving the flags unset
    ttrue(setenv("URL_SHOW", "z", 1) == 0);
    up = urlAlloc(0);
    ttrue(up->flags == URL_SHOW_NONE);
    urlFree(up);

    //  Explicit flags take precedence over the environment
    ttrue(setenv("URL_SHOW", "HBhb", 1) == 0);
    up = urlAlloc(URL_SHOW_RESP_HEADERS);
    ttrue(up->flags == URL_SHOW_RESP_HEADERS);
    urlFree(up);

    ttrue(unsetenv("URL_SHOW") == 0);
    up = urlAlloc(0);
    ttrue(up->flags == 0);
    urlFree(up);
}

/*
    urlSetFlags applies the same flags to an existing object
 */
static void testSetFlags()
{
    Url  *up;
    char url[128];

    up = urlAlloc(0);
    urlSetFlags(up, URL_SHOW_RESP_HEADERS);
    ttrue(up->flags == URL_SHOW_RESP_HEADERS);

    ttrue(urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL) == 200);
    urlFree(up);
}


static void testNoTimeout()
{
    Url  *up;
    char url[128];

    up = urlAlloc(0);
    urlSetTimeout(up, 0);
    ttrue(urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL) == 200);
    ttrue(up->deadline == 0);

    urlSetTimeout(up, -1);
    ttrue(urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL) == 200);
    ttrue(up->deadline == MAXINT64);
    urlFree(up);
}

/*
    A negative body length is the legacy ssize signature. It must be treated as zero rather than as
    an enormous unsigned length, which then falls through to measuring the string body.
 */
static void testNegativeBodyLength()
{
    Url  *up;
    char url[128];

    up = urlAlloc(0);
    ttrue(urlFetch(up, "PUT", SFMT(url, "%s/test/put", HTTP), "ignored", (size_t) -1, NULL) == 200);
    teqi((int) stoi(urlGetResponse(up)), (int) slen("ignored"));
    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testShowFlags();
        testShowBinaryBody();
        testShowTextBody();
        testShowEnvironment();
        testSetFlags();
        testNoTimeout();
        testNegativeBodyLength();
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
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
