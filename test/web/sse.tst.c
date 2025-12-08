/*
    sse.tst.c - Unit tests for Server-Sent Events (SSE)

    Tests SSE event streaming from the web server using the /test/event endpoint
    which sends 100 events with event type "test" and data "Event N".

    Coverage:
    - SSE connection establishment
    - Event reception and parsing
    - Event ID handling
    - Event type verification
    - Data content verification
    - Connection completion

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

typedef struct {
    int eventsReceived;
    int expectedEvents;
    bool verified;
    bool failed;
    char *lastEvent;
    char *lastData;
} SseTestData;

/************************************ Code ************************************/

/*
    SSE callback for basic event test
 */
static void basicEventCallback(Url *up, ssize id, cchar *event, cchar *data, void *arg)
{
    SseTestData *testData = (SseTestData*) arg;
    char        expected[64];

    if (!event || !data) {
        return;
    }
    // Verify event type
    if (!smatch(event, "test")) {
        testData->failed = true;
        return;
    }
    // Verify event data format
    SFMT(expected, "Event %d", testData->eventsReceived);
    if (!smatch(data, expected)) {
        testData->failed = true;
        return;
    }
    testData->eventsReceived++;

    if (testData->eventsReceived == testData->expectedEvents) {
        testData->verified = true;
    }
}

/*
    Test basic SSE event reception using high-level API
 */
static void testBasicEvents(void)
{
    SseTestData testData;
    char        url[128];
    int         rc;

    memset(&testData, 0, sizeof(testData));
    testData.expectedEvents = 100;

    rc = urlGetEvents(SFMT(url, "%s/test/event", HTTP), basicEventCallback, &testData, NULL);

    teqi(rc, 0);
    teqi(testData.eventsReceived, 100);
    ttrue(testData.verified);
    tfalse(testData.failed);
}

/*
    SSE callback for low-level API test
 */
static void lowLevelCallback(Url *up, ssize id, cchar *event, cchar *data, void *arg)
{
    SseTestData *testData = (SseTestData*) arg;

    if (!event || !data) {
        return;
    }
    testData->eventsReceived++;

    // Store last event info for verification
    rFree(testData->lastEvent);
    rFree(testData->lastData);
    testData->lastEvent = sclone(event);
    testData->lastData = sclone(data);

    if (testData->eventsReceived == testData->expectedEvents) {
        testData->verified = true;
    }
}

/*
    Test SSE using low-level URL API
 */
static void testLowLevelApi(void)
{
    SseTestData testData;
    Url         *up;
    char        url[128];
    int         rc;

    memset(&testData, 0, sizeof(testData));
    testData.expectedEvents = 100;

    up = urlAlloc(0);
    ttrue(up != NULL);

    rc = urlStart(up, "GET", SFMT(url, "%s/test/event", HTTP));
    teqi(rc, 0);

    rc = urlWriteHeaders(up, NULL);
    teqi(rc, 0);

    rc = urlFinalize(up);
    teqi(rc, 0);

    // Verify we got HTTP 200 OK
    teqi(urlGetStatus(up), 200);

    // Run SSE event loop
    rc = urlSseRun(up, lowLevelCallback, &testData, up->rx, rGetTicks() + 30 * TPS);
    teqi(rc, 0);

    teqi(testData.eventsReceived, 100);
    ttrue(testData.verified);

    // Verify last event was "Event 99"
    tmatch(testData.lastEvent, "test");
    tmatch(testData.lastData, "Event 99");

    rFree(testData.lastEvent);
    rFree(testData.lastData);
    urlFree(up);
}

/*
    SSE callback that counts events
 */
static void countCallback(Url *up, ssize id, cchar *event, cchar *data, void *arg)
{
    int *count = (int*) arg;

    if (event && data) {
        (*count)++;
    }
}

/*
    Test SSE with HTTPS
 */
static void testHttpsEvents(void)
{
    char url[128];
    int  count, rc;

    count = 0;
    rc = urlGetEvents(SFMT(url, "%s/test/event", HTTPS), countCallback, &count, NULL);

    teqi(rc, 0);
    teqi(count, 100);
}

/*
    Test SSE response headers
 */
static void testResponseHeaders(void)
{
    Url  *up;
    char url[128];
    int  rc;

    up = urlAlloc(0);
    ttrue(up != NULL);

    rc = urlStart(up, "GET", SFMT(url, "%s/test/event", HTTP));
    teqi(rc, 0);

    rc = urlWriteHeaders(up, NULL);
    teqi(rc, 0);

    rc = urlFinalize(up);
    teqi(rc, 0);

    // Verify HTTP status
    teqi(urlGetStatus(up), 200);

    // Verify SSE content type
    tmatch(urlGetHeader(up, "Content-Type"), "text/event-stream");

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testBasicEvents();
        testLowLevelApi();
        testHttpsEvents();
        testResponseHeaders();
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
