/*
    keep-alive.c.tst - Unit tests for keep-alive

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

static void keepAliveTest(void)
{
    Url  *up;
    Json *json1, *json2;
    int  conn1, conn2, count1, count2;
    char url[128];
    int  i;

    up = urlAlloc(0);
    json1 = urlJson(up, "GET", SFMT(url, "%s/test/show", HTTP), NULL, 0, NULL);
    conn1 = jsonGetInt(json1, 0, "connection", -1);
    count1 = jsonGetInt(json1, 0, "count", -1);

    for (i = 0; i < 100; i++) {
        json2 = urlJson(up, "GET", SFMT(url, "%s/test/show", HTTP), NULL, 0, NULL);
        conn2 = jsonGetInt(json2, 0, "connection", -1);
        count2 = jsonGetInt(json2, 0, "count", -1);
        teqi(conn1, conn2);
        teqi(count2, count1 + 1);
        jsonFree(json2);
        count1 = count2;
    }
    jsonFree(json1);

    urlFree(up);
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, &HTTPS)) {
        keepAliveTest();
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
