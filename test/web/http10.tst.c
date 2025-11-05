

/*
    http10.c.tst - Unit tests for HTTP/1.0 requests

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void request()
{
    Url   *up;
    Json  *json;
    cchar *response;
    char  url[128];
    int   status;

    up = urlAlloc(0);
    urlSetProtocol(up, 0);

    //  Static fetch
    status = urlFetch(up, "POST", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    ttrue(status == 200);
    response = urlGetResponse(up);
    tcontains(response, "Hello /index.htm");
    ttrue(sstarts(response, "<html>"));
    tcontains(response, "</html>");

    //  Empty form
    json = urlJson(up, "POST", SFMT(url, "%s/test/show", HTTP), NULL, 0, NULL);
    ttrue(json);
    tmatch(jsonGet(json, 0, "url", 0), "/test/show");
    jsonFree(json);

    //  Simple string
    json = urlJson(up, "POST", SFMT(url, "%s/test/show", HTTP), "\"Hello World\"", (size_t) -1, NULL);
    ttrue(json);
    tmatch(jsonGet(json, 0, "body", 0), "\"Hello World\"");
    jsonFree(json);

    //  URL encoded
    json = urlJson(up, "POST", SFMT(url, "%s/test/show", HTTP), "name=John&zip=98103", (size_t) -1,
                   "Content-Type: application/x-www-form-urlencoded\r\n");
    ttrue(json);
    tmatch(jsonGet(json, 0, "form.name", 0), "John");
    tmatch(jsonGet(json, 0, "form.zip", 0), "98103");
    jsonFree(json);

    //  JSON encoded
    json = urlJson(up, "POST", SFMT(url, "%s/test/show", HTTP), "{\"name\":\"John\",\"zip\":98103}", (size_t) -1,
                   "Content-Type: application/json\r\n");
    ttrue(json);
    tmatch(jsonGet(json, 0, "form.name", 0), "John");
    tmatch(jsonGet(json, 0, "form.zip", 0), "98103");
    jsonFree(json);

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        request();
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
