/*
    leak.h - Exact live-allocation measurement for leak regression tests

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#ifndef _h_DB_TEST_LEAK
#define _h_DB_TEST_LEAK 1

/********************************** Includes **********************************/

#include    "testme.h"
#include    "r.h"

#if defined(__APPLE__)
    #include    <malloc/malloc.h>
    #include    <unistd.h>
#elif defined(__linux__)
    #include    <malloc.h>
#endif

/************************************ Locals **********************************/

static bool leakCheckOk = 0;

/************************************ Code ************************************/
/*
    Enable exact live-byte accounting before other work. May re-exec on macOS; return whether measurement is available.
 */
static bool leakCheckInit(char **argv)
{
#if defined(__APPLE__)
    cchar *nano;

    nano = getenv("MallocNanoZone");
    if (!nano || !smatch(nano, "0")) {
        setenv("MallocNanoZone", "0", 1);
        fflush(stdout);
        execvp(argv[0], argv);
        tinfo("Cannot re-exec %s without the nano allocator: skipping the leak check", argv[0]);
        return 0;
    }
    leakCheckOk = 1;
#elif defined(__linux__)
    leakCheckOk = 1;
#else
    tinfo("Skipping leak check: no malloc statistics API for this platform");
#endif
    return leakCheckOk;
}

/*
    True if leakBytesInUse is exact on this platform
 */
static bool leakCheckEnabled(void)
{
    return leakCheckOk;
}

/*
    Bytes allocated and not yet freed
 */
static ssize leakBytesInUse(void)
{
#if defined(__APPLE__)
    malloc_statistics_t stats;

    malloc_zone_statistics(NULL, &stats);
    return (ssize) stats.size_in_use;
#elif defined(__linux__)
    struct mallinfo2 info;

    info = mallinfo2();
    return (ssize) info.uordblks;
#else
    return 0;
#endif
}

#endif /* _h_DB_TEST_LEAK */

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
