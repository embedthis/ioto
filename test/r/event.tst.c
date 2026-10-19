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
static int fastCount = 0;

/************************************ Code ************************************/

static void eventProc(cchar *signal)
{
    eventCount++;
    tnotnull(signal);
    rSignalSync(signal, "done");
}

/*
    A fast event callback runs on the main fiber and must not yield, so it only counts.
 */
static void fastProc(void *arg)
{
    fastCount++;
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
static void *outsideProc(void *arg)
{
    cchar *signal = (cchar*) arg;

    //  Run an event back inside the Safe runtime
    rStartEvent((REventProc) insideProc, (void*) signal, 0);
    return NULL;
}


static void outsideEvent(void)
{
    cchar *result, *signal;

    //  This will spawn the thread and yield until it completes and return the proc return value
    signal = "outside-signal";
    if (rCreateThread("runtime", (void*) outsideProc, (void*) signal) < 0) {
        tfail();
    } else {
        rWatch(signal, (RWatchProc) rResumeFiber, rGetFiber());
        result = rYieldFiber(0);
        tnotnull(result);
        tmatch(result, "done");
    }
}


/*
    The TestMe comparison macros expand their arguments twice, so every call with a side effect
    is bound to a local before it is asserted.
 */

/*
    rLookupEvent reports whether an id is still pending. It is the only way to tell a scheduled
    event from one that has already run or been stopped.
 */
static void lookupEvent(void)
{
    REvent event;
    bool   pending;
    int    status;

    event = rStartEvent((REventProc) eventProc, "lookup-signal", 5000);
    tneqz(event, 0);
    pending = rLookupEvent(event);
    ttrue(pending);

    status = rStopEvent(event);
    teqi(status, 0);
    pending = rLookupEvent(event);
    tfalse(pending);

    /*
        An id that never existed, and the invalid id zero.
     */
    pending = rLookupEvent(1234567);
    tfalse(pending);
    pending = rLookupEvent(0);
    tfalse(pending);

    status = rStopEvent(0);
    teqi(status, R_ERR_CANT_FIND);
}


/*
    rRunEvent brings a scheduled event forward so it runs now.

    The event has to be relinked, not just have its "when" reassigned: the list is sorted by
    "when" and rGetNextDueEvent returns only the head, so an event left at its old position
    would wait behind whatever is scheduled ahead of it. The blocker below is scheduled nearer
    than the event under test precisely so that a reassign-in-place would fail this test.
 */
static void runEventEarly(void)
{
    REvent event, blocker;
    cchar  *result;
    Ticks  due;
    bool   pending;
    int    count, status;

    count = eventCount;

    blocker = rStartEvent((REventProc) eventProc, "blocker-signal", 5000);
    tneqz(blocker, 0);
    event = rStartEvent((REventProc) eventProc, "run-signal", 3600 * 1000);
    tneqz(event, 0);
    teqi(eventCount, count);

    rWatch("run-signal", (RWatchProc) rResumeFiber, rGetFiber());
    status = rRunEvent(event);
    teqi(status, 0);

    /*
        The event is now at the head of the list and due immediately, ahead of the blocker.
     */
    due = rGetNextDueEvent();
    ttrue(due - rGetTicks() <= 0);
    pending = rLookupEvent(event);
    ttrue(pending);

    /*
        Without the relink this would block until the harness timeout.
     */
    result = rYieldFiber(0);
    tmatch(result, "done");
    teqi(eventCount, count + 1);
    rWatchOff("run-signal", (RWatchProc) rResumeFiber, rGetFiber());

    pending = rLookupEvent(event);
    tfalse(pending);

    status = rStopEvent(blocker);
    teqi(status, 0);

    /*
        An event that has already run cannot be brought forward again.
     */
    status = rRunEvent(event);
    teqi(status, R_ERR_CANT_FIND);
    status = rRunEvent(1234567);
    teqi(status, R_ERR_CANT_FIND);
}


/*
    An event with a short delay runs on its own, which is the path the runtime actually uses.
 */
static void delayedEvent(void)
{
    REvent event;
    cchar  *result;
    int    count;

    count = eventCount;
    rWatch("delay-signal", (RWatchProc) rResumeFiber, rGetFiber());
    event = rStartEvent((REventProc) eventProc, "delay-signal", 20);
    tneqz(event, 0);

    result = rYieldFiber(0);
    tmatch(result, "done");
    teqi(eventCount, count + 1);
    rWatchOff("delay-signal", (RWatchProc) rResumeFiber, rGetFiber());
}


/*
    A fast event runs directly on the main fiber rather than on one of its own, so its callback
    must not yield. The count is checked after the loop has had a chance to turn.
 */
static void fastEvent(void)
{
    REvent event;
    int    count, status;

    count = fastCount;
    event = rStartFastEvent((REventProc) fastProc, NULL, 0);
    tneqz(event, 0);
    teqi(fastCount, count);

    rSleep(50);
    teqi(fastCount, count + 1);

    /*
        A fast event can be stopped before it runs like any other.
     */
    event = rStartFastEvent((REventProc) fastProc, NULL, 5000);
    tneqz(event, 0);
    status = rStopEvent(event);
    teqi(status, 0);
    rSleep(20);
    teqi(fastCount, count + 1);
}


/*
    rGetNextDueEvent reports when the event loop next has work. With nothing scheduled it is the
    end of time, so the loop can block indefinitely.
 */
static void nextDueEvent(void)
{
    REvent event;
    Time   due, later;
    int    status;

    due = rGetNextDueEvent();

    event = rStartEvent((REventProc) eventProc, "due-signal", 3600 * 1000);
    tneqz(event, 0);
    later = rGetNextDueEvent();
    ttrue(later > rGetTicks());
    ttrue(later <= due);

    status = rStopEvent(event);
    teqi(status, 0);
}


static int watchCount;
static cchar *watchArg;

static void watchProc(cvoid *data, cvoid *arg)
{
    watchCount++;
    watchArg = (cchar*) arg;
}


/*
    Watches are keyed on the (name, proc, data) triple. Registering the same triple twice must
    not deliver twice, and rWatchOff must remove exactly the one it names.
 */
static void watches(void)
{
    watchCount = 0;
    watchArg = NULL;

    rWatch("watch-test", watchProc, (void*) 1);
    rSignalSync("watch-test", "payload");
    teqi(watchCount, 1);
    tmatch(watchArg, "payload");

    /*
        The same triple again is a duplicate and is ignored.
     */
    rWatch("watch-test", watchProc, (void*) 1);
    watchCount = 0;
    rSignalSync("watch-test", "payload");
    teqi(watchCount, 1);

    /*
        A different data value is a different watcher, so both fire.
     */
    rWatch("watch-test", watchProc, (void*) 2);
    watchCount = 0;
    rSignalSync("watch-test", "payload");
    teqi(watchCount, 2);

    rWatchOff("watch-test", watchProc, (void*) 2);
    watchCount = 0;
    rSignalSync("watch-test", "payload");
    teqi(watchCount, 1);

    rWatchOff("watch-test", watchProc, (void*) 1);
    watchCount = 0;
    rSignalSync("watch-test", "payload");
    teqi(watchCount, 0);

    /*
        Removing a watcher that was never added, and signalling a name nobody watches, are both
        no-ops rather than errors.
     */
    rWatchOff("watch-test", watchProc, (void*) 99);
    rWatchOff("no-such-name", watchProc, (void*) 1);
    rSignalSync("no-such-name", "payload");
    teqi(watchCount, 0);
    rSignal("no-such-name");
    teqi(watchCount, 0);
}


/*
    rSignal is the asynchronous form: it schedules an event per watcher rather than calling them
    inline, so the count only moves once the loop has run.
 */
static void asyncSignal(void)
{
    watchCount = 0;
    rWatch("async-test", watchProc, (void*) 1);

    rSignal("async-test");
    teqi(watchCount, 0);

    /*
        Yield long enough for the scheduled events to be serviced.
     */
    rSleep(50);
    teqi(watchCount, 1);

    rWatchOff("async-test", watchProc, (void*) 1);
}


/*
    Events that are not at the head of the queue. Every existing case scheduled one event and
    acted on it, so the unlink-from-the-middle branches in rStopEvent, rRunEvent and rRunEvents
    had never run, nor had the saturating delay.
 */
static int cornerFired;

static void cornerProc(void *arg)
{
    cornerFired++;
}

static void eventCorners(void)
{
    REvent distant, soon, mid;

    //  A delay so large the deadline would overflow saturates instead
    distant = rStartEvent(cornerProc, NULL, MAXINT64);
    ttrue(distant > 0);

    //  Stop and run events that sit behind another in the sorted queue
    soon = rStartEvent(cornerProc, NULL, 3600 * TPS);
    mid = rStartEvent(cornerProc, NULL, 7200 * TPS);
    teqi(rStopEvent(mid), 0);
    teqi(rLookupEvent(mid), 0);

    cornerFired = 0;
    teqi(rRunEvent(distant), 0);
    rSleep(20);
    teqi(cornerFired, 1);

    //  Two events due at once are both unlinked in the same pass
    cornerFired = 0;
    rStartEvent(cornerProc, NULL, 1);
    rStartEvent(cornerProc, NULL, 1);
    rSleep(30);
    teqi(cornerFired, 2);

    rStopEvent(soon);
}


static void fiberMain(void *arg)
{
    startEvent();
    stopEvent();
    lookupEvent();
    runEventEarly();
    delayedEvent();
    fastEvent();
    nextDueEvent();
    watches();
    asyncSignal();
    spawnThread();
    outsideEvent();
    eventCorners();
    rStop();
}


int main(void)
{
    rInit((RFiberProc) fiberMain, 0);
    rServiceEvents();
    rTerm();
}


/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a
    commercial license from Embedthis Pty Ltd. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.
 */
