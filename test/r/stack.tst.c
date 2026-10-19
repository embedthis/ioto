/**
    stack.tst.c - Unit tests for growable fiber stacks

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "testme.h"
#include "r.h"

/*********************************** Locals ***********************************/

static volatile int    recurseDepth = 0;
static volatile int    fiberCallCount = 0;
static volatile size_t initialCommitted = 0;
static volatile size_t finalCommitted = 0;

/************************************ Code ************************************/

#if ME_FIBER_GROWABLE_STACK

static void testPageSize(void)
{
    size_t pageSize;

    pageSize = rGetPageSize();
    ttrue(pageSize > 0);
    ttrue(pageSize == 4096 || pageSize == 16384);
}

static void testStackLimitsDefaults(void)
{
    size_t initial, max, grow, resetLimit;

    rGetFiberStackLimits(&initial, &max, &grow, &resetLimit);

    ttrue(initial >= ME_FIBER_MIN_STACK);
    ttrue(max >= initial);
    ttrue(grow > 0);
    ttrue(resetLimit > 0);
}

static void testSetStackLimits(void)
{
    size_t initial, max, grow, resetLimit;
    size_t origInitial, origMax, origGrow, origResetLimit;

    // Save original values
    rGetFiberStackLimits(&origInitial, &origMax, &origGrow, &origResetLimit);

    // Set only initial (others unchanged with 0)
    rSetFiberStackLimits(64 * 1024, 0, 0, 0);
    rGetFiberStackLimits(&initial, &max, &grow, &resetLimit);
    teqz(initial, (size_t) (64 * 1024));
    teqz(max, origMax);
    teqz(grow, origGrow);
    teqz(resetLimit, origResetLimit);

    // Set all values
    rSetFiberStackLimits(32 * 1024, 512 * 1024, 8 * 1024, 48 * 1024);
    rGetFiberStackLimits(&initial, &max, &grow, &resetLimit);
    teqz(initial, (size_t) (32 * 1024));
    teqz(max, (size_t) (512 * 1024));
    // Note: grow and resetLimit are aligned to page size
    ttrue(grow >= 8 * 1024);
    ttrue(resetLimit >= 48 * 1024);

    // Restore original values
    rSetFiberStackLimits(origInitial, origMax, origGrow, origResetLimit);
}

static void testStackLimitsMinimum(void)
{
    size_t initial, max, grow, resetLimit;
    size_t origInitial, origMax, origGrow, origResetLimit;

    // Save original values
    rGetFiberStackLimits(&origInitial, &origMax, &origGrow, &origResetLimit);

    // Try to set below minimum - should be clamped to ME_FIBER_MIN_STACK
    rSetFiberStackLimits(1024, 0, 0, 0);  // 1K is below minimum
    rGetFiberStackLimits(&initial, &max, &grow, &resetLimit);
    ttrue(initial >= ME_FIBER_MIN_STACK);

    // Restore original values
    rSetFiberStackLimits(origInitial, origMax, origGrow, origResetLimit);
}

static void simpleFiber(void *data)
{
    fiberCallCount++;
}

static void testGuardPageAlloc(void)
{
    RFiber *fiber;

    fiberCallCount = 0;
    fiber = rAllocFiber("test", simpleFiber, NULL);
    tnotnull(fiber);
    // On Windows/pthreads, uctx manages its own stacks so guarded is false
#if UCTX_ARCH != UCTX_WINDOWS && UCTX_ARCH != UCTX_PTHREADS && !FREERTOS
    ttrue(fiber->stackInfo.guarded);
    tnotnull(fiber->stackInfo.base);
    tnotnull(fiber->stackInfo.usable);
    tnotnull(fiber->stackInfo.top);
    ttrue(fiber->stackInfo.committed > 0);
    ttrue(fiber->stackInfo.reserved >= fiber->stackInfo.committed);
#endif

    rFreeFiber(fiber);
}

static void testPageApis(void)
{
    void   *ptr;
    size_t pageSize;

    pageSize = rGetPageSize();

    // Test rAllocPages reserves memory
    ptr = rAllocPages(pageSize * 4);
    tnotnull(ptr);

    // Test rProtectPages commits memory
    teqi(rProtectPages(ptr, pageSize, R_PROT_READ | R_PROT_WRITE), 0);

    // Write to committed page should succeed
    memset(ptr, 0x42, pageSize);

    // Clean up
    rFreePages(ptr, pageSize * 4);
}

static void testPoolReusesGuardedStack(void)
{
    RFiber *fiber1, *fiber2;
    void   *originalBase;

    // Allocate first fiber
    fiber1 = rAllocFiber("pool1", simpleFiber, NULL);
    tnotnull(fiber1);
    originalBase = fiber1->stackInfo.base;

    // Return to pool
    rFreeFiber(fiber1);

    // Allocate second fiber - should get same stack from pool
    fiber2 = rAllocFiber("pool2", simpleFiber, NULL);
    tnotnull(fiber2);

    // Stack base should be reused from pool
    teqp(fiber2->stackInfo.base, originalBase);

    rFreeFiber(fiber2);
}

// Fiber-based tests run inside the event loop
static void moderateRecurse(int depth)
{
    volatile char buffer[1024];  // Force stack usage

    buffer[0] = (char) depth;
    recurseDepth = depth;
    if (depth < 20) {
        moderateRecurse(depth + 1);
    }
}

static void recurseFiber(void *data)
{
    recurseDepth = 0;
    moderateRecurse(0);
}

// Deep recursion to force stack growth beyond initial allocation
static void deepRecurse(int depth)
{
    volatile char buffer[4096];  // 4KB per frame to force growth

    buffer[0] = (char) depth;
    buffer[4095] = (char) depth;
    recurseDepth = depth;
    if (depth < 15) {
        deepRecurse(depth + 1);
    }
}

static void deepRecurseFiber(void *data)
{
    RFiber *fiber = rGetFiber();

    initialCommitted = fiber->stackInfo.committed;
    recurseDepth = 0;
    deepRecurse(0);
    finalCommitted = fiber->stackInfo.committed;
}

static void runFiber(void *data)
{
    fiberCallCount++;
}

static void testFiberRunsInternal(void)
{
    RFiber *fiber;
    int    count;

    count = fiberCallCount;
    fiber = rAllocFiber("test-run", runFiber, NULL);
    tnotnull(fiber);

    rStartFiber(fiber, NULL);
    rSleep(50);

    teqi(fiberCallCount, count + 1);
}

static void testModerateRecursionInternal(void)
{
    RFiber *fiber;

    recurseDepth = 0;
    fiber = rAllocFiber("recurse", recurseFiber, NULL);
    tnotnull(fiber);

    rStartFiber(fiber, NULL);
    rSleep(100);

    // Should have completed recursion within initial stack
    ttrue(recurseDepth >= 10);
}

static void testStackGrowthInternal(void)
{
    RFiber *fiber;

    initialCommitted = 0;
    finalCommitted = 0;
    recurseDepth = 0;

    fiber = rAllocFiber("deep-recurse", deepRecurseFiber, NULL);
    tnotnull(fiber);

    rStartFiber(fiber, NULL);
    rSleep(200);

    // Should have completed deep recursion
    ttrue(recurseDepth >= 10);

    // Stack should have grown beyond initial allocation
    // 16 levels * 4KB = 64KB minimum, initial is 32KB
    // Only check on platforms where we manage stacks (not Windows/pthreads/FreeRTOS)
#if UCTX_ARCH != UCTX_WINDOWS && UCTX_ARCH != UCTX_PTHREADS && !FREERTOS
    ttrue(finalCommitted > initialCommitted);
    ttrue(finalCommitted > ME_FIBER_DEFAULT_STACK);
#endif
}

/*
    A stack that grew past the reset limit is decommitted back to its initial size when the fiber
    returns to the pool, so one deep call does not leave every pooled fiber holding a large stack.
    The reset limit is lowered for the duration so the fixture does not have to recurse as far as
    the default would need.

    The TestMe comparison macros expand their arguments twice, so every call with a side effect
    is bound to a local before it is asserted.
 */
static void testStackResetOnPooling(void)
{
    RFiber *fiber;
    size_t origInitial, origMax, origGrow, origReset;
    size_t committed;

    rGetFiberStackLimits(&origInitial, &origMax, &origGrow, &origReset);

    /*
        Reset anything committed beyond the initial size. The limit is a "greater than" test, so
        it has to sit below what the recursion will commit for the reset to fire at all.
     */
    rSetFiberStackLimits(0, 0, 0, origInitial);

    initialCommitted = 0;
    finalCommitted = 0;
    recurseDepth = 0;

    fiber = rAllocFiber("reset-recurse", deepRecurseFiber, NULL);
    tnotnull(fiber);
    rStartFiber(fiber, NULL);
    rSleep(200);

    ttrue(recurseDepth >= 10);
    //  Commit accounting is only ours on platforms where we manage stacks
#if UCTX_ARCH != UCTX_WINDOWS && UCTX_ARCH != UCTX_PTHREADS && !FREERTOS
    ttrue(finalCommitted > rGetPageSize());
#endif

    /*
        The fiber has completed and gone back to the pool. Taking it out again must hand back a
        stack trimmed to its initial size rather than the one it grew to: the whole point of the
        reset is that one deep call does not leave every pooled fiber holding a large stack.
     */
    fiber = rAllocFiber("reset-check", runFiber, NULL);
    tnotnull(fiber);
    committed = fiber->stackInfo.committed;
#if UCTX_ARCH != UCTX_WINDOWS && UCTX_ARCH != UCTX_PTHREADS && !FREERTOS
    ttrue(committed <= origInitial);
    ttrue(committed < finalCommitted);
#endif
    rFreeFiber(fiber);

    rSetFiberStackLimits(origInitial, origMax, origGrow, origReset);
}


static void fiberMain(void *arg)
{
    testPageSize();
    testStackLimitsDefaults();
    testSetStackLimits();
    testStackLimitsMinimum();
    testGuardPageAlloc();
    testPageApis();
    testPoolReusesGuardedStack();

    testFiberRunsInternal();
    testModerateRecursionInternal();
    testStackGrowthInternal();
    testStackResetOnPooling();
    rStop();
}

#endif /* ME_FIBER_GROWABLE_STACK */

int main(void)
{
#if ME_FIBER_GROWABLE_STACK
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
#endif
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
