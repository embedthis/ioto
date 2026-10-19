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


static void *threadProc(void *arg)
{
    RFiber *fiber = (RFiber*) arg;

    /*
        Foreign thread. Very few Safe runtime routines are thread safe.
        Those that are thread safe: rStartEvent, rResumeFiber, rStartFiber and the thread.c routines
     */
    rResumeFiber(fiber, "thread-result");
    return NULL;
}


static void testStartThread()
{
    cchar *result;

    if (rCreateThread("test-thread", (void*) threadProc, rGetFiber()) != 0) {
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


/*
    The TestMe comparison macros expand their arguments twice, so every call below with a side
    effect is bound to a local before it is asserted.
 */

/*
    rTryLock must never block. The locks are recursive, so a second acquire on the same thread
    succeeds and has to be released as many times as it was taken.
 */
static void testTryLock(void)
{
    RLock *lp;
    bool  got;

    lp = rAllocLock();
    tnotnull(lp);

    got = rTryLock(lp);
    ttrue(got);

    /*
        Recursive: the owning thread may take it again rather than deadlocking on itself.
     */
    got = rTryLock(lp);
    ttrue(got);
    rUnlock(lp);
    rUnlock(lp);

    /*
        Free again afterwards.
     */
    got = rTryLock(lp);
    ttrue(got);
    rUnlock(lp);

    got = rTryLock(NULL);
    tfalse(got);

    rFreeLock(lp);
}


/*
    A lock held by another thread cannot be taken. This is the case rTryLock exists for, and it
    is the only one that needs a second thread.

    The helper touches nothing but pthread primitives and two volatile ints - the runtime is not
    thread safe, and a foreign thread may not call into it.
 */
static volatile int helperHolding;
static volatile int helperRelease;
static RLock        *contended;

static void *holderProc(void *arg)
{
    rLock(contended);
    helperHolding = 1;
    while (!helperRelease) {
        usleep(1000);
    }
    rUnlock(contended);
    helperHolding = 0;
    return NULL;
}


static void testTryLockContended(void)
{
    bool got;
    int  i, rc;

    contended = rAllocLock();
    tnotnull(contended);
    helperHolding = 0;
    helperRelease = 0;

    rc = rCreateThread("lock-holder", (void*) holderProc, NULL);
    if (rc != 0) {
        tinfo("Skipped: Cannot create the holder thread");
        rFreeLock(contended);
        return;
    }
    /*
        Wait for the helper to take the lock. rSleep yields the fiber, so the runtime keeps
        running while the other thread makes progress.
     */
    for (i = 0; i < 500 && !helperHolding; i++) {
        rSleep(2);
    }
    ttrue(helperHolding);

    got = rTryLock(contended);
    tfalse(got, "A lock held by another thread must not be acquired");

    helperRelease = 1;
    for (i = 0; i < 500 && helperHolding; i++) {
        rSleep(2);
    }
    tfalse(helperHolding);

    /*
        Once released it is available again.
     */
    got = rTryLock(contended);
    ttrue(got);
    rUnlock(contended);

    rFreeLock(contended);
    contended = 0;
}


/*
    The thread identity accessors. Every test in this file runs on the main thread, so the two
    must agree here - and must not be null, since callers use them as identity tokens.
 */
static void testThreadIdentity(void)
{
    RThread current, main;

    current = rGetCurrentThread();
    main = rGetMainThread();
    ttrue(current == main);

    /*
        Stable across calls.
     */
    ttrue(rGetCurrentThread() == current);

    /*
        A barrier is a no-op to observe but must be safe to call.
     */
    rMemoryBarrier();
    ttrue(1);
}


/*
    rInitLock initialises a caller-supplied lock rather than allocating one, which is what a
    static or embedded lock needs. rTermLock releases it without freeing it.
 */
static void testStaticLock(void)
{
    RLock lock;
    RLock *lp;
    bool  got;

    memset(&lock, 0, sizeof(lock));
    lp = rInitLock(&lock);
    ttrue(lp == &lock);

    got = rTryLock(&lock);
    ttrue(got);
    rUnlock(&lock);

    rLock(&lock);
    rUnlock(&lock);

    rTermLock(&lock);

    /*
        The global lock is the same machinery under a fixed name.
     */
    rGlobalLock();
    rGlobalUnlock();
    ttrue(1);
}


static void fiberMain(void *arg)
{
    initLock();
    criticalSection();
    termLock();
    testSpawnThread();
    testStartThread();

    testTryLock();
    testTryLockContended();
    testThreadIdentity();
    testStaticLock();
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
