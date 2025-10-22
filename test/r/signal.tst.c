/**
    signal-test.tst.c - Unit tests for sigal/watch

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "r.h"

/************************************ Code ************************************/

static void signalEvent(cchar *signal)
{
    tnotnull(signal);
    rSignalSync(signal, "done");
}

static void resumeFiber(RFiber *fiber, void *arg)
{
    rResumeFiber(fiber, arg);
}

static void signalTest(void)
{
    cchar *result, *signal;

    signal = "signal-test";
    rWatch(signal, (RWatchProc) resumeFiber, rGetFiber());
    rStartEvent((REventProc) signalEvent, (void*) signal, 10);

    result = rYieldFiber(0);
    tnotnull(result);
    tmatch(result, "done");
}

static void fiberMain()
{
    signalTest();
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
