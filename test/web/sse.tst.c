/*
    websockets.c.tst - Unit tests for HTTPS requests

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"
#include    "web.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;
static char  *WS;
static int   count = 0;
static int   expected = 0;

/************************************ Code ************************************/

static void onEvent(WebSocket *ws, int event, cchar *buf, size_t len, void *arg)
{
    if (event == WS_EVENT_OPEN || event == WS_EVENT_MESSAGE) {
        //  Echo
        if (++count == expected) {
            webSocketSendClose(ws, WS_STATUS_OK, "OK");
        } else {
            webSocketSend(ws, "Message %d", count);
        }
    }
}

static void highLevelAPI()
{
    Url  *up;
    char url[128];
    int  rc;

    up = urlAlloc(0);
    count = 0;
    expected = 2;
    rc = urlWebSocket(SFMT(url, "%s/test/ws/", WS), onEvent, NULL, NULL);
    ttrue(rc == 0);
    ttrue(count == 2);
    urlFree(up);
}

static void lowLevelAPI()
{
    Url  *up;
    char url[128];
    int  rc;

    count = 0;
    expected = 10;

    up = urlAlloc(0);
    rc = urlStart(up, "GET", SFMT(url, "%s/test/ws/", WS));
    ttrue(rc == 0);

    rc = urlWriteHeaders(up, NULL);
    ttrue(rc == 0);

    rc = urlFinalize(up);
    ttrue(rc == 0);

    urlWebSocketAsync(up, onEvent, up);

    rc = urlWait(up);
    ttrue(rc == 0);

    ttrue(count == 10);
    urlFree(up);
}

static void testFiber()
{
    if (setup(&HTTP, &HTTPS)) {
        WS = sreplace(HTTP, "http", "ws");
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
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
