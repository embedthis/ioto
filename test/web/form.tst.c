

/*
    form.c.tst - Unit tests for form requests

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void form()
{
    Json *json;
    Url  *up;
    char url[128];

    up = urlAlloc(0);

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
    tcontains(jsonGet(json, 0, "form.name", 0), "John");
    tcontains(jsonGet(json, 0, "form.zip", 0), "98103");
    jsonFree(json);

    //  JSON encoded
    json = urlJson(up, "POST", SFMT(url, "%s/test/show", HTTP), "{\"name\":\"John\",\"zip\":98103}", (size_t) -1,
                   "Content-Type: application/json\r\n");
    ttrue(json);
    tcontains(jsonGet(json, 0, "form.name", 0), "John");
    tcontains(jsonGet(json, 0, "form.zip", 0), "98103");
    jsonFree(json);

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        form();
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
