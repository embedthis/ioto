/**
    event-test.tst.c - Unit tests for events

    WARNING: The Safe Runtime is not thread-safe in general

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "r.h"

/*********************************** Locals ***********************************/

static int eventCount = 0;

/************************************ Code ************************************/

static void eventProc(cchar *signal)
{
    eventCount++;
    tnotnull(signal);
    rSignalSync(signal, "done");
}

//  Test rStartEvent
static void startEvent(void)
{
    REvent event;
    cchar  *result;

    event = rStartEvent((REventProc) eventProc, "event-signal", 50);
    tneqz(event, 0);
    rWatch("event-signal", (RWatchProc) rResumeFiber, rGetFiber());
    result = rYieldFiber(0);
    tmatch(result, "done");
}


static void stopEvent(void)
{
    REvent event;
    int    count, status;

    count = eventCount;
    event = rStartEvent((REventProc) eventProc, "event-signal", 0);
    tneqz(event, 0);
    teqi(eventCount, count);

    // Safe to stop unknown events
    status = rStopEvent(1234567);
    teqi(status, R_ERR_CANT_FIND);
    teqi(eventCount, count);

    //  Stop and the event should never run
    status = rStopEvent(event);
    teqi(status, 0);
    teqi(eventCount, count);

    //  Sleep to ensure event won't run
    rSleep(50);
    teqi(eventCount, count);
}


static cchar *spawnMain(void)
{
    //  Runs on an outside thread. WARNING: cannot call most runtime APIs
    return "outsideProc";
}


static void spawnThread(void)
{
    cchar *result;

    //  This will spawn the thread and yield until it completes and return the proc return value
    if ((result = rSpawnThread((RThreadProc) spawnMain, 0)) == 0) {
        tfail();
    } else {
        tnotnull(result);
        tmatch(result, "outsideProc");
    }
}


//  Runs on an fiber inside
static void insideProc(cchar *signal)
{
    tnotnull(signal);
    rSignalSync(signal, "done");
}


//  Runs on an outside thread. WARNING: cannot call most runtime APIs
static void outsideProc(cchar *signal)
{
    //  Run an event back inside the Safe runtime
    rStartEvent((REventProc) insideProc, (void*) signal, 0);
}


static void outsideEvent(void)
{
    cchar *result, *signal;

    //  This will spawn the thread and yield until it completes and return the proc return value
    signal = "outside-signal";
    if (rCreateThread("runtime", outsideProc, (void*) signal) < 0) {
        tfail();
    } else {
        rWatch(signal, (RWatchProc) rResumeFiber, rGetFiber());
        result = rYieldFiber(0);
        tnotnull(result);
        tmatch(result, "done");
    }
}


static void fiberMain()
{
    startEvent();
    stopEvent();
    spawnThread();
    outsideEvent();
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
    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.
 */
