/**
    thread-test.tst.c - Unit tests for threads and locking

    WARNING: The Safe Runtime is not thread-safe in general

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "r.h"

/*********************************** Locals ***********************************/

static RThread critical[2048];

/*
    Single global lock
 */
static RLock *lock;
static int   threadCount;

/************************************ Code ************************************/

static int initLock()
{
    rGlobalLock();
    if (lock == 0) {
        lock = rAllocLock();
    }
    threadCount++;
    rGlobalUnlock();
    return 0;
}


static int termLock()
{
    rGlobalLock();
    if (--threadCount == 0) {
        if (lock) {
            rFreeLock(lock);
            lock = 0;
        }
    }
    rGlobalUnlock();
    return 0;
}


static void criticalSection()
{
    int i, size;

    rLock(lock);
    tnotnull(lock);
    size = sizeof(critical) / sizeof(RThread);
    for (i = 0; i < size; i++) {
        critical[i] = rGetCurrentThread();
    }
    for (i = 0; i < size; i++) {
        ttrue(critical[i] == rGetCurrentThread());
    }
    rUnlock(lock);
    tnotnull(lock);
}


static void threadProc(RFiber *fiber)
{
    /*
        Foreign thread. Very few Safe runtime routines are thread safe.
        Those that are thread safe: rStartEvent, rResumeFiber, rStartFiber and the thread.c routines
     */
    rResumeFiber(fiber, "thread-result");
}


static void testStartThread()
{
    cchar *result;

    if (rCreateThread("test-thread", (RThreadProc) threadProc, rGetFiber()) != 0) {
        tfail();
    } else {
        ttrue(1);
        result = rYieldFiber(0);
        tnotnull(result);
        tmatch(result, "thread-result");
    }
}


static cchar *spawnProc(void *data)
{
    tnotnull(data);
    tmatch(data, "99");
    //  Will automatically resume the spawning thread
    return "spawn-result";
}


static void testSpawnThread(void)
{
    cchar *result;

    //  This will spawn the thread and yield until it completes
    if ((result = rSpawnThread((RThreadProc) spawnProc, "99")) == 0) {
        tfail();
    } else {
        tnotnull(result);
        tmatch(result, "spawn-result");
    }
}


static void fiberMain(void *arg)
{
    initLock();
    criticalSection();
    termLock();
    testSpawnThread();
    testStartThread();
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
