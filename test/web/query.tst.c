

/*
    query.c.tst - Unit tests for query parsing

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void query()
{
    Url  *up;
    Json *json;
    char url[128];

    up = urlAlloc(0);

    //  Key=value
    json = urlJson(up, "POST", SFMT(url, "%s/test/show?a=1&b=2&c=3", HTTP), NULL, 0, NULL);
    ttrue(json);
    tmatch(jsonGet(json, 0, "query.a", 0), "1");
    tmatch(jsonGet(json, 0, "query.b", 0), "2");
    tmatch(jsonGet(json, 0, "query.c", 0), "3");
    jsonFree(json);

    //  arg,arg,
    json = urlJson(up, "POST", SFMT(url, "%s/test/show?a&b&c", HTTP), NULL, 0, NULL);
    ttrue(json);
    tmatch(jsonGet(json, 0, "query.a", 0), "");
    tmatch(jsonGet(json, 0, "query.b", 0), "");
    tmatch(jsonGet(json, 0, "query.c", 0), "");
    jsonFree(json);

    urlFree(up);
}


static void encoded()
{
    Url  *up;
    Json *json;
    char url[128];

    up = urlAlloc(0);

    //  Key=value
    json =
        urlJson(up, "POST", SFMT(url, "%s/test/show?greeting=hello%%20world&address=44%%20Smith%%26Parker%%20Ave",
                                 HTTP), NULL, 0, NULL);
    ttrue(json);
    tmatch(jsonGet(json, 0, "query.greeting", 0), "hello world");
    tmatch(jsonGet(json, 0, "query.address", 0), "44 Smith&Parker Ave");
    jsonFree(json);

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        query();
        encoded();
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
