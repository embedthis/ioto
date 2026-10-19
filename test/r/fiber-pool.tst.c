/**
    fiber-pool.tst.c - Unit tests for fiber pool

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "testme.h"
#include "r.h"

/*********************************** Locals ***********************************/

static int fiberCallCount = 0;

/************************************ Code ************************************/

static void simpleFiber(void *data)
{
    fiberCallCount++;
    rYieldFiber(NULL);
}

static void testPoolHitMiss(void)
{
    RFiber *fiber1, *fiber2;
    int    active, max, pooled, poolMax, poolMin;
    uint64 hits, misses, origHits, origMisses;

    rGetFiberStats(&active, &max, &pooled, &poolMax, &poolMin, &origHits, &origMisses);

    // Allocate first fiber - should miss (new allocation)
    fiber1 = rAllocFiber("test1", simpleFiber, NULL);
    tnotnull(fiber1);

    rGetFiberStats(&active, &max, &pooled, &poolMax, &poolMin, &hits, &misses);
    teqll(misses, origMisses + 1);

    // Free fiber - should go to pool
    rFreeFiber(fiber1);
    rGetFiberStats(&active, &max, &pooled, &poolMax, &poolMin, &hits, &misses);
    tgtez(pooled, 1);

    // Allocate second fiber - should hit (from pool)
    fiber2 = rAllocFiber("test2", simpleFiber, NULL);
    tnotnull(fiber2);

    rGetFiberStats(&active, &max, &pooled, &poolMax, &poolMin, &hits, &misses);
    teqll(hits, origHits + 1);

    rFreeFiber(fiber2);
}

static void testPoolOverflow(void)
{
    RFiber *fibers[20];
    int    active, max, pooled, poolMax, poolMin, origPooled, i;
    uint64 hits, misses;

    rGetFiberStats(&active, &max, &pooled, &poolMax, &poolMin, &hits, &misses);
    origPooled = pooled;

    // Allocate more fibers than pool limit
    for (i = 0; i < poolMax + 5; i++) {
        fibers[i] = rAllocFiber("overflow", simpleFiber, NULL);
        tnotnull(fibers[i]);
    }

    // Free all - pool should fill to limit, rest go to heap
    for (i = 0; i < poolMax + 5; i++) {
        rFreeFiber(fibers[i]);
    }

    rGetFiberStats(&active, &max, &pooled, &poolMax, &poolMin, &hits, &misses);
    teqi(pooled, poolMax);  // Pool should be at limit
}

static void testPoolReuse(void)
{
    RFiber *fiber;
    cchar  *signal;
    int    count, i;

    signal = "pool-reuse";
    count = fiberCallCount;

    // Rapid alloc/free cycles should reuse pooled fibers
    for (i = 0; i < 20; i++) {
        fiber = rAllocFiber("reuse", simpleFiber, NULL);
        tnotnull(fiber);
        rStartFiber(fiber, NULL);
    }

    // Wait for fibers to run
    rSleep(100);

    // All fibers should have run
    teqi(fiberCallCount, count + 20);
}

static void testSetPoolSize(void)
{
    int    active, max, pooled, poolMax, poolMin;
    uint64 hits, misses;

    // Get original values
    rGetFiberStats(&active, &max, &pooled, &poolMax, &poolMin, &hits, &misses);
    teqi(poolMax, 8);  // Default pool limit
    teqi(poolMin, 1);  // Default pool min (ME_FIBER_POOL_MIN)

    // Change pool size (keep fiber limit at 0 = unlimited)
    rSetFiberLimits(0, 2, 4);
    rGetFiberStats(&active, &max, &pooled, &poolMax, &poolMin, &hits, &misses);
    teqi(poolMax, 4);
    teqi(poolMin, 2);

    // Restore original
    rSetFiberLimits(0, 1, 8);
}

static void testPoolStats(void)
{
    int    active, max, pooled, poolMax, poolMin;
    uint64 hits, misses;

    rGetFiberStats(&active, &max, &pooled, &poolMax, &poolMin, &hits, &misses);

    // Verify stats are sane
    tgtez(active, 0);
    tgtez(pooled, 0);
    tgtez(poolMax, 0);
    tgtez(poolMin, 0);
    tltez(pooled, poolMax);
    tltez(poolMin, poolMax);
}

static void fiberMain(void *arg)
{
    testPoolStats();
    testPoolHitMiss();
    testPoolOverflow();
    testPoolReuse();
    testSetPoolSize();
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
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
