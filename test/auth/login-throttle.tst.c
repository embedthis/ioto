/*
    login-throttle.tst.c - Repeated failed logins from one client are refused with 429, without holding a fiber

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "auth.h"

/************************************ Code ************************************/

static void testThrottle(void)
{
    Ticks start, elapsed;
    char  *cookie, *log, url[256];
    int   i, status;

    waitFailurePeriod();
    start = rGetTicks();
    for (i = 0; i < MAX_FAILURES; i++) {
        teqi(postLogin("guest", "wrong-password", NULL, NULL), 401);
    }
    elapsed = rGetTicks() - start;
    tinfo("%d failures in %lld msec", MAX_FAILURES, (long long) elapsed);
    ttrue(elapsed < 1000, "A failure must be answered at once, not after a delay");

    status = postLogin("guest", FIXTURE_PASSWORD, NULL, &cookie);
    teqi(status, 429, "The correct password is refused once the limit is reached");
    tnull(cookie);
    status = postLogin("nosuchuser", "wrong-password", NULL, NULL);
    teqi(status, 429);

    //  Documents are still served to the refused client
    ttrue(expect("GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL, 200));

    log = readLog();
    tcontains(log, "too many failures (route '/api/public/')");
    rFree(log);

    //  After the failure period the client may log in again
    rSleep(FAILURE_PERIOD);
    status = postLogin("guest", FIXTURE_PASSWORD, NULL, &cookie);
    teqi(status, 302);
    tnotnull(cookie);
    rFree(cookie);
}

static void fiberMain(void *data)
{
    if (scenarioSetup()) {
        testThrottle();
    }
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
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
