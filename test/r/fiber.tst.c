/**
    fiber.tst.c - Unit tests for fiber coroutines

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "r.h"

/*********************************** Locals ***********************************/

static int    fiberCallCount = 0;
static RFiber *savedFiber = NULL;

/************************************ Code ************************************/

//  Simple fiber that increments a counter and yields
static void simpleFiber(void *data)
{
    int *count;

    count = (int*) data;
    tnotnull(count);
    (*count)++;
    rYieldFiber(NULL);
}

//  Test basic fiber allocation and execution
static void testSimpleFiber(void)
{
    RFiber *fiber;
    int    count;

    count = 0;
    fiber = rAllocFiber("simple-fiber", simpleFiber, &count);
    tnotnull(fiber);
    teqi(count, 0);

    //  Start the fiber via event system since we're in a fiber context
    rStartFiber(fiber, NULL);

    //  Yield to allow the fiber to run
    rSleep(10);
    teqi(count, 1);
}

//  Fiber that yields multiple times and signals when done
static void multiYieldFiber(void *data)
{
    int   i;
    cchar *signal;

    signal = (cchar*) data;
    fiberCallCount++;
    for (i = 0; i < 5; i++) {
        rSleep(10);
    }
    rSignalSync(signal, "done");
}

//  Test fiber that yields multiple times
static void testMultiYield(void)
{
    RFiber *fiber;
    cchar  *result, *signal;
    int    count;

    count = fiberCallCount;
    signal = "multi-yield-signal";
    fiber = rAllocFiber("multi-yield", multiYieldFiber, (void*) signal);
    tnotnull(fiber);

    rStartFiber(fiber, NULL);
    rWatch(signal, (RWatchProc) rResumeFiber, rGetFiber());
    result = rYieldFiber(0);
    tmatch(result, "done");
    teqi(fiberCallCount, count + 1);
}

//  Fiber that receives and validates data via signals
static void dataPassingFiber(void *data)
{
    int   *counter;
    cchar *signal;

    counter = (int*) data;
    signal = "data-signal";

    tnotnull(data);
    (*counter)++;

    //  Signal back to caller
    rSignalSync(signal, "fiber-complete");
}

//  Test passing data to/from fibers
static void testDataPassing(void)
{
    RFiber *fiber;
    cchar  *result, *signal;
    int    count;

    count = 0;
    signal = "data-signal";
    fiber = rAllocFiber("data-passing", dataPassingFiber, &count);
    tnotnull(fiber);

    rStartFiber(fiber, NULL);
    rWatch(signal, (RWatchProc) rResumeFiber, rGetFiber());
    result = rYieldFiber(0);
    tnotnull(result);
    tmatch(result, "fiber-complete");
    teqi(count, 1);
}

//  Fiber that spawns another fiber
static void nestedFiber(void *data)
{
    int *count;

    count = (int*) data;
    (*count)++;
    rSignalSync("nested-signal", "nested-done");
}

static void parentFiber(void *data)
{
    int   *count;
    cchar *result;

    count = (int*) data;
    (*count) *= 10;

    //  Spawn a nested fiber and wait for it
    rSpawnFiber("nested", nestedFiber, data);
    rWatch("nested-signal", (RWatchProc) rResumeFiber, rGetFiber());
    result = rYieldFiber(0);
    tmatch(result, "nested-done");

    //  Signal parent is complete
    rSignalSync("parent-signal", "parent-done");
}

//  Test nested fiber spawning
static void testNestedFibers(void)
{
    RFiber *fiber;
    cchar  *result;
    int    count;

    count = 2;
    fiber = rAllocFiber("parent-fiber", parentFiber, &count);
    tnotnull(fiber);

    rStartFiber(fiber, NULL);
    rWatch("parent-signal", (RWatchProc) rResumeFiber, rGetFiber());
    result = rYieldFiber(0);
    tmatch(result, "parent-done");

    //  Count should be: (2 * 10) + 1 = 21
    teqi(count, 21);
}

//  Fiber for testing rSpawnFiber
static void spawnedFiber(void *data)
{
    cchar *str;

    str = (cchar*) data;
    tnotnull(str);
    tmatch(str, "spawn-test");
    fiberCallCount++;
    rYieldFiber(NULL);
}

//  Test rSpawnFiber convenience function
static void testSpawnFiber(void)
{
    int count, status;

    count = fiberCallCount;
    status = rSpawnFiber("spawned-fiber", spawnedFiber, "spawn-test");
    teqi(status, 0);

    //  Sleep to allow spawned fiber to run
    rSleep(50);
    teqi(fiberCallCount, count + 1);
}

//  Fiber that sleeps
static void sleepingFiber(void *data)
{
    Ticks start, elapsed;
    cchar *signal;

    signal = (cchar*) data;
    start = rGetTicks();

    //  Sleep for 100ms
    rSleep(100);

    elapsed = rGetTicks() - start;
    //  Allow some tolerance for timing
    tgtez(elapsed, 90);

    rSignalSync(signal, "done");
}

//  Test fiber sleep functionality
static void testFiberSleep(void)
{
    RFiber *fiber;
    cchar  *result, *signal;

    signal = "sleep-signal";
    fiber = rAllocFiber("sleeping-fiber", sleepingFiber, (void*) signal);
    tnotnull(fiber);

    rStartFiber(fiber, NULL);
    rWatch(signal, (RWatchProc) rResumeFiber, rGetFiber());
    result = rYieldFiber(0);
    tmatch(result, "done");
}

//  Fiber for testing rGetFiber
static void getCurrentFiber(void *data)
{
    RFiber *current;

    current = rGetFiber();
    tnotnull(current);
    ttrue(current == savedFiber);
    rSignalSync("getfiber-signal", "done");
}

//  Test rGetFiber function
static void testGetFiber(void)
{
    RFiber *fiber, *currentFiber;
    cchar  *result;

    //  Get current fiber (we're in fiberMain)
    currentFiber = rGetFiber();
    tnotnull(currentFiber);

    //  Allocate a new fiber
    fiber = rAllocFiber("get-fiber", getCurrentFiber, NULL);
    tnotnull(fiber);
    savedFiber = fiber;

    //  Start the fiber - it will check if rGetFiber returns itself
    rStartFiber(fiber, NULL);
    rWatch("getfiber-signal", (RWatchProc) rResumeFiber, rGetFiber());
    result = rYieldFiber(0);
    tmatch(result, "done");

    //  Back in our fiber
    ttrue(rGetFiber() == currentFiber);
}

//  Test rIsMain function
static void checkIsMain(void *data)
{
    int *notMain;

    notMain = (int*) data;
    //  Inside a non-main fiber, should not be main
    *notMain = !rIsMain();
    rSignalSync("ismain-signal", "done");
}

static void testIsMain(void)
{
    RFiber *fiber;
    cchar  *result;
    int    notMain;

    //  We're in fiberMain fiber, so should not be main
    tfalse(rIsMain());

    notMain = 0;
    fiber = rAllocFiber("check-main", checkIsMain, &notMain);
    tnotnull(fiber);

    rStartFiber(fiber, NULL);
    rWatch("ismain-signal", (RWatchProc) rResumeFiber, rGetFiber());
    result = rYieldFiber(0);
    tmatch(result, "done");
    teqi(notMain, 1);

    //  Back in fiberMain, still not main
    tfalse(rIsMain());
}

//  Test fiber stack functions
static void testFiberStack(void)
{
    void   *stack;
    size_t stackSize;

    //  Get stack info from main fiber
    stack = rGetFiberStack();
    if (uctx_needstack()) {
        tnotnull(stack);
    } else {
        tnull(stack);
    }
    stackSize = rGetFiberStackSize();
    //  Stack should be at least 16K (minimum) and typically 64K
    tgtez(stackSize, 16 * 1024);
}

//  Test setting fiber stack size
static void stackSizeFiber(void *data)
{
    size_t *size;

    size = (size_t*) data;
    *size = rGetFiberStackSize();
    rSignalSync("stacksize-signal", "done");
}

static void testSetFiberStack(void)
{
    RFiber *fiber;
    cchar  *result;
    size_t originalSize, newSize, reportedSize;

    originalSize = rGetFiberStackSize();
    tgtez(originalSize, 0);

    //  Set a larger stack size
    newSize = 128 * 1024;
    rSetFiberStackSize(newSize);
    teqz(rGetFiberStackSize(), newSize);

    //  Create fiber with new stack size
    reportedSize = 0;
    fiber = rAllocFiber("stack-size", stackSizeFiber, &reportedSize);
    tnotnull(fiber);
    rStartFiber(fiber, NULL);
    rWatch("stacksize-signal", (RWatchProc) rResumeFiber, rGetFiber());
    result = rYieldFiber(0);
    tmatch(result, "done");
    teqz(reportedSize, newSize);

    //  Restore original stack size
    rSetFiberStackSize(originalSize);
    teqz(rGetFiberStackSize(), originalSize);
}

/*
    Fiber for testing critical sections
 */
static void criticalFiber(void *data)
{
    typedef struct {
        bool *access;
        int fiberNum;
        cchar *signal;
    } CriticalData;

    CriticalData *cd;
    int          status;
    Ticks        deadline;

    cd = (CriticalData*) data;

    //  Try to enter critical section with timeout
    deadline = rGetTicks() + 500;
    status = rEnter(cd->access, deadline);
    teqi(status, 0);

    //  In critical section
    rSleep(30);

    //  Leave critical section
    rLeave(cd->access);

    //  Signal completion
    rSignalSync(cd->signal, "done");
}

//  Test fiber critical sections
static void testCriticalSection(void)
{
    typedef struct {
        bool *access;
        int fiberNum;
        cchar *signal;
    } CriticalData;

    RFiber       *fiber1, *fiber2;
    CriticalData data1, data2;
    cchar        *result;
    bool         access;

    access = 0;

    data1.access = &access;
    data1.fiberNum = 1;
    data1.signal = "critical1-signal";

    data2.access = &access;
    data2.fiberNum = 2;
    data2.signal = "critical2-signal";

    fiber1 = rAllocFiber("critical-1", criticalFiber, &data1);
    tnotnull(fiber1);

    fiber2 = rAllocFiber("critical-2", criticalFiber, &data2);
    tnotnull(fiber2);

    //  Start both fibers - they will coordinate via the access flag
    rStartFiber(fiber1, NULL);
    rStartFiber(fiber2, NULL);

    //  Wait for both to complete
    rWatch("critical1-signal", (RWatchProc) rResumeFiber, rGetFiber());
    result = rYieldFiber(0);
    tmatch(result, "done");

    rWatch("critical2-signal", (RWatchProc) rResumeFiber, rGetFiber());
    result = rYieldFiber(0);
    tmatch(result, "done");

    //  Access should be free now
    tfalse(access);
}

/*
    The TestMe comparison macros expand their arguments twice, so every call below with a side
    effect is bound to a local before it is asserted.
 */

/*
    rEnter waits for the flag to clear. With a deadline already in the past it must give up and
    report rather than spin, and it must not take the section.
 */
static void testEnterTimeout(void)
{
    bool  access;
    int   status;
    Ticks deadline;

    access = 0;
    status = rEnter(&access, rGetTicks() + 100);
    teqi(status, 0);
    ttrue(access);

    /*
        The section is held and the deadline has passed, so the second entry is refused and the
        holder keeps it.
     */
    deadline = rGetTicks() - 1;
    status = rEnter(&access, deadline);
    teqi(status, R_ERR_CANT_ACCESS);
    ttrue(access);

    rLeave(&access);
    tfalse(access);

    /*
        Once free, entry succeeds again even with a deadline in the past: the deadline is only
        consulted while waiting.
     */
    status = rEnter(&access, rGetTicks() - 1);
    teqi(status, 0);
    rLeave(&access);
}


/*
    The fiber exception block.

    This is a two-part protocol and the halves live on either side of the library boundary.
    rStartFiberBlock arms the fiber; the CALLER then calls setjmp on the fiber's own jmpbuf, in
    the caller's frame, which is the only place setjmp can legally be called from. The runtime
    cannot do it on the caller's behalf, because setjmp's frame must still be live when longjmp
    fires. The handlers in fiber.c longjmp into that buffer when a fiber faults.

    web/src/http.c is the real user of this, guarded by its "web.fiberBlocks" setting:

        rStartFiberBlock();
        blockResult = setjmp(rGetFiber()->jmpbuf);
        if (blockResult == 0) {
            ... serve the request ...
        } else {
            rEndFiberBlock();
            ... report and close the connection ...
        }

    Both arms are exercised below.
 */
static void blockFiber(void *data)
{
    int *count;

    count = (int*) data;

    /*
        A block with no exception in it: arm, run, disarm. Nothing faults, so setjmp returns
        zero once and the else arm never runs.
     */
    rStartFiberBlock();
    if (setjmp(rGetFiber()->jmpbuf) == 0) {
        (*count)++;
    } else {
        rEndFiberBlock();
        *count = -1;
    }
    rSignalSync("block-signal", "done");
}


static void testFiberBlock(void)
{
    RFiber *fiber;
    cchar  *result;
    int    count;

    count = 0;
    fiber = rAllocFiber("block-fiber", blockFiber, &count);
    tnotnull(fiber);

    rWatch("block-signal", (RWatchProc) rResumeFiber, rGetFiber());
    rStartFiber(fiber, NULL);
    result = rYieldFiber(0);
    tmatch(result, "done");
    teqi(count, 1);
    rWatchOff("block-signal", (RWatchProc) rResumeFiber, rGetFiber());
}


/*
    The exception arm. A fiber that faults inside an armed block must be delivered back to its
    setjmp with a non-zero result rather than aborting the process, and the statement after the
    fault must never run.

    This is the only path that reaches fiberSignalHandler and the exception half of
    guardPageHandler, so without it those stay at zero however much else is covered.
 */
static volatile int blockFaultReached;
static volatile int blockFaultCaught;
static volatile int blockFaultAfter;

static void faultingFiber(void *data)
{
    volatile int *bad;
    int          blockResult;

    bad = NULL;

    rStartFiberBlock();
    blockResult = setjmp(rGetFiber()->jmpbuf);

    if (blockResult == 0) {
        blockFaultReached = 1;
        /*
            Deliberate null dereference. The handler longjmps back to the setjmp above.
         */
        *bad = 42;
        blockFaultAfter = 1;
    } else {
        rEndFiberBlock();
        blockFaultCaught = blockResult;
    }
    rSignalSync("fault-signal", "recovered");
}


static void testFiberBlockCatchesFault(void)
{
    RFiber *fiber;
    cchar  *result;

    blockFaultReached = 0;
    blockFaultCaught = 0;
    blockFaultAfter = 0;

    fiber = rAllocFiber("faulting", faultingFiber, NULL);
    tnotnull(fiber);

    rWatch("fault-signal", (RWatchProc) rResumeFiber, rGetFiber());
    rStartFiber(fiber, NULL);
    result = rYieldFiber(0);
    tmatch(result, "recovered");
    rWatchOff("fault-signal", (RWatchProc) rResumeFiber, rGetFiber());

    teqi(blockFaultReached, 1);

    /*
        setjmp returned the signal number, not zero, so the fault was delivered as an exception.
     */
    ttrue(blockFaultCaught != 0, "A fault inside an armed block must return through setjmp");

    /*
        And the statement after the fault never executed.
     */
    teqi(blockFaultAfter, 0);
}


//  Fiber that aborts part way through. The code after the abort must never run.
static void abortingFiber(void *data)
{
    int *count;

    count = (int*) data;
    (*count)++;

    rSignalSync("abort-signal", "aborting");
    rAbortFiber();

    /*
        Unreachable. If rAbortFiber ever returns, this makes it visible.
     */
    (*count) += 100;
}


static void testAbortFiber(void)
{
    RFiber *fiber;
    cchar  *result;
    int    count, active, before;

    rGetFiberStats(&before, NULL, NULL, NULL, NULL, NULL, NULL);

    count = 0;
    fiber = rAllocFiber("abort-fiber", abortingFiber, &count);
    tnotnull(fiber);

    rWatch("abort-signal", (RWatchProc) rResumeFiber, rGetFiber());
    rStartFiber(fiber, NULL);
    result = rYieldFiber(0);
    tmatch(result, "aborting");
    rWatchOff("abort-signal", (RWatchProc) rResumeFiber, rGetFiber());

    /*
        Let the abort complete. The fiber ran once and the tail never executed.
     */
    rSleep(20);
    teqi(count, 1);

    /*
        An aborted fiber is freed outright rather than returned to the pool, so the active count
        comes back to where it started.
     */
    rGetFiberStats(&active, NULL, NULL, NULL, NULL, NULL, NULL);
    teqi(active, before);
}


/*
    rSetFiberStackSize clamps to the module minimum and ignores a zero.
 */
static void testStackSizeClamping(void)
{
    size_t original, size;

    original = rGetFiberStackSize();
    ttrue(original > 0);

    rSetFiberStackSize(0);
    size = rGetFiberStackSize();
    teqz(size, original);

    /*
        Below the minimum: clamped up, not accepted and not rejected.
     */
    rSetFiberStackSize(1024);
    size = rGetFiberStackSize();
    ttrue(size > 1024);

    rSetFiberStackSize(original);
    size = rGetFiberStackSize();
    teqz(size, original);
}


/*
    rSetFiberLimits returns the previous maximum and clamps poolMax to it. Every limit is put
    back before returning so nothing later in the file runs against a constrained pool.
 */
static void testFiberLimits(void)
{
    int max, poolMin, poolMax, prior, rc;

    rGetFiberStats(NULL, &max, NULL, &poolMax, &poolMin, NULL, NULL);

    rc = rSetFiberLimits(-1, 0, 0);
    teqi(rc, R_ERR_BAD_ARGS);
    rc = rSetFiberLimits(0, -1, 0);
    teqi(rc, R_ERR_BAD_ARGS);
    rc = rSetFiberLimits(0, 0, -1);
    teqi(rc, R_ERR_BAD_ARGS);

    /*
        A rejected call changes nothing.
     */
    rGetFiberStats(NULL, &prior, NULL, NULL, NULL, NULL, NULL);
    teqi(prior, max);

    /*
        poolMax cannot exceed the fiber maximum.
     */
    prior = rSetFiberLimits(4, 1, 100);
    teqi(prior, max);
    rGetFiberStats(NULL, &prior, NULL, &poolMax, &poolMin, NULL, NULL);
    teqi(prior, 4);
    teqi(poolMax, 4);
    teqi(poolMin, 1);

    /*
        A maximum of zero means unlimited, so poolMax is left alone.
     */
    prior = rSetFiberLimits(0, 2, 7);
    teqi(prior, 4);
    rGetFiberStats(NULL, &prior, NULL, &poolMax, &poolMin, NULL, NULL);
    teqi(prior, 0);
    teqi(poolMax, 7);
    teqi(poolMin, 2);

    rSetFiberLimits(max, poolMin, poolMax);
}


/*
    With the fiber count already at its limit, allocation is refused rather than exceeding it.
 */
static void testFiberLimitReached(void)
{
    RFiber *fiber;
    int    max, poolMin, poolMax, active;

    rGetFiberStats(&active, &max, NULL, &poolMax, &poolMin, NULL, NULL);

    /*
        Set the ceiling to exactly what is in use, so the next allocation has nowhere to go.
     */
    rSetFiberLimits(active, poolMin, poolMax);

    fiber = rAllocFiber("over-limit", simpleFiber, NULL);
    tnull(fiber);

    /*
        One more slot is enough for the allocation to succeed again.
     */
    rSetFiberLimits(active + 1, poolMin, poolMax);
    fiber = rAllocFiber("within-limit", simpleFiber, NULL);
    tnotnull(fiber);
    rFreeFiber(fiber);

    rSetFiberLimits(max, poolMin, poolMax);
}


/*
    The statistics accessor. Every pointer is optional, so a caller can ask for one number.
 */
static void testFiberStats(void)
{
    uint64 hits, misses;
    int    active, max, pooled, poolMax, poolMin;

    rGetFiberStats(&active, &max, &pooled, &poolMax, &poolMin, &hits, &misses);
    ttrue(active >= 1);
    ttrue(max >= 0);
    ttrue(pooled >= 0);
    ttrue(poolMax >= 0);
    ttrue(poolMin >= 0);

    /*
        Every fiber this file has run was either taken from the pool or newly allocated.
     */
    ttrue(hits + misses > 0);

    /*
        Asking for nothing is not an error.
     */
    rGetFiberStats(NULL, NULL, NULL, NULL, NULL, NULL, NULL);

    active = -1;
    rGetFiberStats(&active, NULL, NULL, NULL, NULL, NULL, NULL);
    ttrue(active >= 1);
}


/*
    A fiber of the runtime's own is not a foreign thread.
 */
static void testForeignThread(void)
{
    bool foreign;

    foreign = rIsForeignThread();
    tfalse(foreign);
}


//  Main fiber entry point for tests
static void fiberMain(void *arg)
{
    testSimpleFiber();
    testMultiYield();
    testDataPassing();
    testNestedFibers();
    testSpawnFiber();
    testFiberSleep();
    testGetFiber();
    testIsMain();
    testFiberStack();
    testSetFiberStack();
    testCriticalSection();

    testEnterTimeout();
    testFiberBlock();
    testFiberBlockCatchesFault();
    testAbortFiber();
    testStackSizeClamping();
    testFiberLimits();
    testFiberLimitReached();
    testFiberStats();
    testForeignThread();
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
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
