/*
    sse.c.tst - Server-Sent Events

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char    *HTTP;
static char    *HTTPS;
static int count = 0;
static int expected = 0;

/************************************ Code ************************************/

static void onEvent(Url *url, ssize id, cchar *event, cchar *data, void *arg)
{
    count++;
    tinfo("test", "SSE event %d: id=%zd event=%s data=%s", count, id, event ? event : "NULL", data ? data : "NULL");
}

static void highLevelAPI()
{
    char    url[128];
    int     rc;

    count = 0;
    expected = 100;

    //  Expect 100 events from web test.c
    rc = urlGetEvents(SFMT(url, "%s/test/event", HTTP), onEvent, NULL, NULL);

    ttrue(rc == 0);
    tinfo("test", "highLevelAPI: count=%d expected=100", count);
    ttrue(count == 100);
}

static void lowLevelAPI()
{
    Url     *up;
    char    url[128];
    int     rc;

    count = 0;
    expected = 100;

    up = urlAlloc(0);

    rc = urlStart(up, "GET", SFMT(url, "%s/test/event", HTTP));
    ttrue(rc == 0);

    //  Send the request and wait for response status
    rc = urlFinalize(up);
    ttrue(rc == 0);

    //  Use urlSseRun to receive events
    rc = urlSseRun(up, onEvent, up, up->rx, rGetTicks() + 30000);
    ttrue(rc == 0);
    ttrue(count == 100);

    urlFree(up);
}

static void keepAlive()
{
    Url     *up;
    cchar   *response;
    char    url[128];
    int     rc, status;

    count = 0;
    expected = 100;

    up = urlAlloc(0);

    //  Same as per lowLevelAPI
    rc = urlStart(up, "GET", SFMT(url, "%s/test/event", HTTP));
    ttrue(rc == 0);
    rc = urlFinalize(up);
    ttrue(rc == 0);

    //  Use urlSseRun to receive events
    rc = urlSseRun(up, onEvent, up, up->rx, rGetTicks() + 30000);
    ttrue(rc == 0);
    ttrue(count == 100);

    //  Request the connection
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    ttrue(status == URL_CODE_OK);

    response = urlGetResponse(up);
    ttrue(response != NULL);

    urlFree(up);
}

static void runAPI()
{
    Url     *up;
    char    url[128];
    int     rc;

    count = 0;
    expected = 100;

    up = urlAlloc(0);

    rc = urlStart(up, "GET", SFMT(url, "%s/test/event", HTTP));
    ttrue(rc == 0);

    rc = urlFinalize(up);
    ttrue(rc == 0);

    // Use urlSseRun directly
    rc = urlSseRun(up, onEvent, NULL, up->rx, rGetTicks() + 30000);
    ttrue(rc == 0);
    ttrue(count == 100);

    urlFree(up);
}

static void testFiber()
{
    if (setup(&HTTP, &HTTPS)) {
        highLevelAPI();
        lowLevelAPI();
        runAPI();
        keepAlive();
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
