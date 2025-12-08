

/*
    tls.c.tst - Unit tests for HTTPS requests

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void get()
{
    Url   *up;
    cchar *response;
    char  url[128];
    int   status;

    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTPS), NULL, 0, NULL);
    ttrue(status == 200);
    response = urlGetResponse(up);
    tcontains(response, "Hello /index.html");
    ttrue(sstarts(response, "<html>"));
    tcontains(response, "</html>");
    urlFree(up);
}

static void getWithBody()
{
    Url   *up;
    char  url[128];
    cchar *response;
    int   status;

    up = urlAlloc(0);
    status =
        urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTPS), "name=John&address=700+Park+Ave", (size_t) -1, NULL);
    ttrue(status == 200);
    if (status != 200) {
        twrite("Error: %s\n", urlGetError(up));
    } else {
        response = urlGetResponse(up);
        tcontains(response, "Hello /index.htm");
    }
    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        get();
        getWithBody();
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
