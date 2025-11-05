/*
    time.tst.c - Unit tests for Time and Date routines

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "r.h"

/************************************ Code ************************************/

static void timeBasics()
{
    Time  now;
    Ticks mark, remaining, elapsed;

    now = rGetTime();
    tnotnull((void*) (ssize) now);

    mark = rGetTicks();
    tnotnull((void*) (ssize) mark);
    remaining = rGetRemainingTicks(mark, 30000);
    ttrue(0 <= remaining && remaining <= 30000);

    elapsed = rGetElapsedTicks(mark);
    ttrue(0 <= elapsed && elapsed < 30000);

    rSleep(1000);
    now = rGetTime();
    ttrue(now > mark);
}

static void formatTime()
{
    Time now;
    char *str;

    now = rGetTime();
    str = rFormatLocalTime(NULL, now);
    ttrue(str && *str);

    str = rFormatLocalTime("%Y-%m-%d %H:%M:%S", now);
    ttrue(str && *str);
    teqz(slen(str), 19);

    str = rFormatUniversalTime(NULL, now);
    ttrue(str && *str);

    str = rFormatUniversalTime("%Y-%m-%d %H:%M:%S", now);
    ttrue(str && *str);
    teqz(slen(str), 19);
}

static void testGetDate()
{
    char *str;

    str = rGetDate(NULL);
    ttrue(str && *str);

    str = rGetDate("");
    ttrue(str && *str);

    str = rGetDate("%Y-%m-%d");
    ttrue(str && *str);
    teqz(slen(str), 10);

    str = rGetDate("%H:%M:%S");
    ttrue(str && *str);
    teqz(slen(str), 8);
}

static void testIsoDate()
{
    Time now, parsed;
    char *isoStr;

    now = rGetTime();
    isoStr = rGetIsoDate(now);
    ttrue(isoStr && *isoStr);
    ttrue(scontains(isoStr, "T"));
    ttrue(sends(isoStr, "Z"));

    parsed = rParseIsoDate("2023-12-25T10:30:45Z");
    ttrue(parsed > 0);

    parsed = rParseIsoDate("2023-12-25T10:30:45+00:00");
    ttrue(parsed > 0);
}

static void testHiResTicks()
{
    uint64 ticks1, ticks2;

    ticks1 = rGetHiResTicks();
    tgti(ticks1, 0);

    ticks2 = rGetHiResTicks();
    ttrue(ticks2 >= ticks1);
}

static void testTicksEdgeCases()
{
    Ticks mark, remaining, elapsed;

    mark = rGetTicks();
    remaining = rGetRemainingTicks(mark, 0);
    ttrue(remaining <= 0);

    remaining = rGetRemainingTicks(mark, 1000);
    ttrue(remaining <= 1000);

    elapsed = rGetElapsedTicks(mark);
    ttrue(elapsed >= 0);

    elapsed = rGetElapsedTicks(mark + 1000);
    ttrue(elapsed <= 0);
}

static void testFormatEdgeCases()
{
    Time time;
    char *str;

    time = 0;
    str = rFormatLocalTime("%Y", time);
    ttrue(str && *str);

    str = rFormatUniversalTime("%Y", time);
    ttrue(str && *str);

    time = rGetTime();
    str = rFormatLocalTime("", time);
    ttrue(str != NULL);

    str = rFormatUniversalTime("", time);
    ttrue(str != NULL);
}

int main(void)
{
    rInit(0, 0);
    timeBasics();
    formatTime();
    testGetDate();
    testIsoDate();
    testHiResTicks();
    testTicksEdgeCases();
    testFormatEdgeCases();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
