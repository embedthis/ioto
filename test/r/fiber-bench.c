/**
    fiber-bench.c - Fiber allocation/free benchmark

    Not a test - it makes no assertions and has no pass or fail. It was named *.tst.c and
    carried "Skip: manual" so the harness would leave it alone, which meant it was never built
    either. Build and run it with "make bench" from the top level.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "testme.h"
#include "r.h"

/*********************************** Locals ***********************************/

#define BENCH_ITERATIONS 1000

/************************************ Code ************************************/

static void dummyFiber(void *data)
{
    rYieldFiber(NULL);
}

static void testAllocFreePerf(void)
{
    RFiber *fibers[BENCH_ITERATIONS];
    uint64 start, allocElapsed, freeElapsed;
    int    i;

    // Benchmark allocation
    start = rGetHiResTicks();
    for (i = 0; i < BENCH_ITERATIONS; i++) {
        fibers[i] = rAllocFiber("bench", dummyFiber, NULL);
        if (!fibers[i]) {
            tinfo("Failed to allocate fiber %d", i);
            break;
        }
    }
    allocElapsed = rGetHiResTicks() - start;
    tinfo("Alloc %d fibers: %llu ticks, avg %llu ticks/fiber",
          i, (unsigned long long) allocElapsed, (unsigned long long) (allocElapsed / i));

    // Benchmark free
    start = rGetHiResTicks();
    for (int j = 0; j < i; j++) {
        rFreeFiber(fibers[j]);
    }
    freeElapsed = rGetHiResTicks() - start;
    tinfo("Free %d fibers: %llu ticks, avg %llu ticks/fiber",
          i, (unsigned long long) freeElapsed, (unsigned long long) (freeElapsed / i));

    // Combined cycle
    tinfo("Total alloc+free cycle: %llu ticks, avg %llu ticks/cycle",
          (unsigned long long) (allocElapsed + freeElapsed),
          (unsigned long long) ((allocElapsed + freeElapsed) / i));
}

static void testAllocFreeChurn(void)
{
    RFiber *fiber;
    uint64 start, elapsed;
    int    i, iterations;

    iterations = BENCH_ITERATIONS;

    // Benchmark repeated alloc/free (simulates request handling)
    start = rGetHiResTicks();
    for (i = 0; i < iterations; i++) {
        fiber = rAllocFiber("churn", dummyFiber, NULL);
        if (!fiber) {
            tinfo("Failed to allocate fiber %d", i);
            break;
        }
        rFreeFiber(fiber);
    }
    elapsed = rGetHiResTicks() - start;
    tinfo("Churn %d alloc+free cycles: %llu ticks, avg %llu ticks/cycle",
          i, (unsigned long long) elapsed, (unsigned long long) (elapsed / i));
}

static void testPoolStats(void)
{
    int    active, max, pooled, poolMax, poolMin;
    uint64 hits, misses;

    rGetFiberStats(&active, &max, &pooled, &poolMax, &poolMin, &hits, &misses);
    tinfo("Pool stats: pooled=%d, max=%d, min=%d, hits=%llu, misses=%llu",
          pooled, poolMax, poolMin, (unsigned long long) hits, (unsigned long long) misses);
    if (hits + misses > 0) {
        tinfo("Pool hit rate: %.1f%%", (double) hits * 100.0 / (double) (hits + misses));
    }
}

static void fiberMain(void *arg)
{
    tinfo("Starting fiber benchmark");
    tinfo("Stack size: %zu bytes", rGetFiberStackSize());

    testAllocFreePerf();
    testAllocFreeChurn();
    testPoolStats();

    tinfo("Benchmark complete");
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
