/*
    misc.tst.c - Miscellaneous tests including Server-Sent Events

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;
static int  count = 0;

/************************************ Code ************************************/

static void onEvent(Url *url, ssize id, cchar *event, cchar *data, void *arg)
{
    count++;
}

static void highLevelAPI(void)
{
    char url[128];
    int  rc;

    count = 0;
    rc = urlGetEvents(SFMT(url, "%s/test/event", HTTP), onEvent, NULL, NULL);

    teqi(rc, 0);
    teqi(count, 100);
}

static void lowLevelAPI(void)
{
    Url  *up;
    char url[128];
    int  rc;

    count = 0;

    up = urlAlloc(0);

    rc = urlStart(up, "GET", SFMT(url, "%s/test/event", HTTP));
    teqi(rc, 0);

    rc = urlWriteHeaders(up, NULL);
    teqi(rc, 0);

    rc = urlFinalize(up);
    teqi(rc, 0);

    teqi(urlGetStatus(up), 200);

    rc = urlSseRun(up, onEvent, NULL, up->rx, rGetTicks() + 30 * TPS);
    teqi(rc, 0);

    teqi(count, 100);
    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        highLevelAPI();
        lowLevelAPI();
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
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
