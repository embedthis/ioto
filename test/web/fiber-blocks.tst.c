/*
    fiber-blocks.tst.c - Test fiber exception blocks for crash recovery

    Tests that the Web server correctly handles exceptions in handler code
    when fiberBlocks is enabled, allowing the server to continue serving
    other requests after a handler crash.

    Coverage:
    - Null pointer dereference in handler
    - Divide by zero in handler
    - Server recovery after crash
    - Normal requests after crash

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

/************************************ Code ************************************/

/*
    Test that a null pointer crash in a handler doesn't take down the server.
    The connection should be closed, but subsequent requests should succeed.
 */
static void testNullPointerCrash(void)
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    /*
        Make a request that will crash with null pointer dereference
        The server should catch the exception and close the connection
     */
    status = urlFetch(up, "GET", SFMT(url, "%s/test/crash/null", HTTP), NULL, 0, NULL);

    /*
        The request should fail (connection closed by server)
        Status will be negative (connection error) or 0 (incomplete response)
     */
    tinfo("Crash request status: %d", status);
    ttrue(status <= 0 || status >= 500);
    urlFree(up);
}

/*
    Test that the server continues to work after a crash.
 */
static void testServerContinuesAfterCrash(void)
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // First, trigger a crash
    status = urlFetch(up, "GET", SFMT(url, "%s/test/crash/null", HTTP), NULL, 0, NULL);
    tinfo("Crash request status: %d", status);

    // Close the connection to reset state
    urlClose(up);

    // Now make a normal request - the server should still be running
    status = urlFetch(up, "GET", SFMT(url, "%s/test/success", HTTP), NULL, 0, NULL);
    tinfo("Recovery request status: %d", status);
    teqi(status, 200);
    urlFree(up);
}

/*
    Test multiple crashes in sequence - server should handle all of them.
 */
static void testMultipleCrashes(void)
{
    Url  *up;
    char url[128];
    int  status, i;

    for (i = 0; i < 3; i++) {
        up = urlAlloc(0);

        // Trigger a crash
        status = urlFetch(up, "GET", SFMT(url, "%s/test/crash/null", HTTP), NULL, 0, NULL);
        tinfo("Crash %d request status: %d", i + 1, status);
        ttrue(status <= 0 || status >= 500);

        urlClose(up);

        // Verify server is still running
        status = urlFetch(up, "GET", SFMT(url, "%s/test/success", HTTP), NULL, 0, NULL);
        tinfo("Recovery %d request status: %d", i + 1, status);
        teqi(status, 200);

        urlFree(up);
    }
}

/*
    Test divide by zero crash recovery.
 */
static void testDivideByZeroCrash(void)
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);

    // Make a request that will crash with divide by zero
    status = urlFetch(up, "GET", SFMT(url, "%s/test/crash/divide", HTTP), NULL, 0, NULL);
    tinfo("Divide crash request status: %d", status);
    ttrue(status <= 0 || status >= 500);
    urlClose(up);

    // Verify server is still running
    status = urlFetch(up, "GET", SFMT(url, "%s/test/success", HTTP), NULL, 0, NULL);
    tinfo("Recovery request status: %d", status);
    teqi(status, 200);

    urlFree(up);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        if (getenv("TESTME_DEBUGGER") == NULL) {
            testNullPointerCrash();
            testServerContinuesAfterCrash();
            testMultipleCrashes();
            testDivideByZeroCrash();
        } else {
            // Xcode cannot handle these tests
            tinfo("Debugger detected, skipping tests");
        }
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
