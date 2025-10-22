/*
    sse.c.tst - Server-Sent Events

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static cchar    *HTTP;
static cchar    *HTTPS;
static int count = 0;
static int expected = 0;

/************************************ Code ************************************/

static void onEvent(Url *url, ssize id, cchar *event, cchar *data, void *arg)
{
    count++;
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

    //  Set up the callback to receive events
    urlSseAsync(up, onEvent, up);

    /*
        Wait for all the events to arrive. Server will finish the request after 100 events
        Socket remains open for keep alive
     */
    rc = urlWait(up);
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

    //  Sames as per lowLevelAPI
    rc = urlStart(up, "GET", SFMT(url, "%s/test/event", HTTP));
    ttrue(rc == 0);
    rc = urlFinalize(up);
    ttrue(rc == 0);
    urlSseAsync(up, onEvent, up);
    rc = urlWait(up);
    ttrue(rc == 0);
    ttrue(count == 100);

    //  Request the connection
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    ttrue(status == URL_CODE_OK);

    response = urlGetResponse(up);
    ttrue(response != NULL);

    urlFree(up);
}

static void testFiber()
{
    if (setup(&HTTP, &HTTPS)) {
        highLevelAPI();
        lowLevelAPI();
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
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
