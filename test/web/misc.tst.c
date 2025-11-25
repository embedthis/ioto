/*
    sse.c.tst - Server-Sent Events

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;
static int  count = 0;
static int  expected = 0;

/************************************ Code ************************************/

static void onEvent(Url *url, ssize id, cchar *event, cchar *data, void *arg)
{
    count++;
}

static void highLevelAPI()
{
    char url[128];
    int  rc;

    count = 0;
    expected = 100;

    rc = urlGetEvents(SFMT(url, "%s/test/event", HTTP), onEvent, NULL, NULL);

    ttrue(rc == 0);
    ttrue(count == 100);
}

static void lowLevelAPI()
{
    Url  *up;
    char url[128];
    int  rc;

    count = 0;
    expected = 100;

    up = urlAlloc(0);

    rc = urlStart(up, "GET", SFMT(url, "%s/test/event", HTTP));
    ttrue(rc == 0);

    rc = urlFinalize(up);
    ttrue(rc == 0);

    urlSseAsync(up, onEvent, up);

    rc = urlWait(up);
    ttrue(rc == 0);

    ttrue(count == 100);
    urlFree(up);
}

static void testFiber()
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
    rInit(0, 0);
    rSpawnFiber("test", (RFiberProc) testFiber, NULL);
    rServiceEvents();
    rTerm();
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
