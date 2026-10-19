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

    // Test formatting
    now = rGetTime();
    isoStr = rGetIsoDate(now);
    ttrue(isoStr && *isoStr);
    ttrue(scontains(isoStr, "T"));
    ttrue(sends(isoStr, "Z"));

    // Test basic parsing with Z
    parsed = rParseIsoDate("2023-12-25T10:30:45Z");
    tgtll(parsed, 0);

    // Test parsing with +00:00
    parsed = rParseIsoDate("2023-12-25T10:30:45+00:00");
    tgtll(parsed, 0);

    // Test milliseconds preservation with Z
    parsed = rParseIsoDate("2023-12-25T10:30:45.123Z");
    tgtll(parsed, 0);
    teqi((int) (parsed % 1000), 123);

    // Test milliseconds preservation with numeric offset
    parsed = rParseIsoDate("2023-12-25T10:30:45.456+00:00");
    tgtll(parsed, 0);
    teqi((int) (parsed % 1000), 456);

    // Test microseconds (should scale to milliseconds)
    parsed = rParseIsoDate("2023-12-25T10:30:45.123456Z");
    tgtll(parsed, 0);
    teqi((int) (parsed % 1000), 123);

    // Test nanoseconds (should scale to milliseconds)
    parsed = rParseIsoDate("2023-12-25T10:30:45.123456789Z");
    tgtll(parsed, 0);
    teqi((int) (parsed % 1000), 123);

    // Test extra fractional digits beyond 9 (should be ignored)
    parsed = rParseIsoDate("2023-12-25T10:30:45.12345678901234Z");
    tgtll(parsed, 0);
    teqi((int) (parsed % 1000), 123);

    // Test zero milliseconds
    parsed = rParseIsoDate("2023-12-25T10:30:45.000Z");
    tgtll(parsed, 0);
    teqi((int) (parsed % 1000), 0);

    // Test 999 milliseconds
    parsed = rParseIsoDate("2023-12-25T10:30:45.999Z");
    tgtll(parsed, 0);
    teqi((int) (parsed % 1000), 999);

    // Test various timezone offset formats
    parsed = rParseIsoDate("2023-12-25T10:30:45+0000");
    tgtll(parsed, 0);

    parsed = rParseIsoDate("2023-12-25T10:30:45+05:30");
    tgtll(parsed, 0);

    parsed = rParseIsoDate("2023-12-25T10:30:45+0530");
    tgtll(parsed, 0);

    parsed = rParseIsoDate("2023-12-25T10:30:45-08:00");
    tgtll(parsed, 0);

    parsed = rParseIsoDate("2023-12-25T10:30:45-0800");
    tgtll(parsed, 0);

    // Test milliseconds with various offsets
    parsed = rParseIsoDate("2023-12-25T10:30:45.789+05:30");
    tgtll(parsed, 0);
    teqi((int) (parsed % 1000), 789);

    parsed = rParseIsoDate("2023-12-25T10:30:45.999-08:00");
    tgtll(parsed, 0);
    teqi((int) (parsed % 1000), 999);

    // Test that equivalent times produce same result
    // "2023-12-25T10:30:45Z" and "2023-12-25T10:30:45+00:00" should be equal
    Time t1 = rParseIsoDate("2023-12-25T10:30:45Z");
    Time t2 = rParseIsoDate("2023-12-25T10:30:45+00:00");
    teqll(t1, t2);

    // Test timezone offset calculation
    // "2023-12-25T10:30:45+01:00" should be 1 hour earlier than "2023-12-25T10:30:45Z" in UTC
    Time utc = rParseIsoDate("2023-12-25T10:30:45Z");
    Time plusOne = rParseIsoDate("2023-12-25T10:30:45+01:00");
    teqll(utc - plusOne, 3600 * 1000);       // 1 hour in milliseconds

    // "2023-12-25T10:30:45-05:00" should be 5 hours later than "2023-12-25T10:30:45Z" in UTC
    Time minusFive = rParseIsoDate("2023-12-25T10:30:45-05:00");
    teqll(minusFive - utc, 5 * 3600 * 1000); // 5 hours in milliseconds

    // Test round-trip: format and parse should preserve time
    now = rGetTime();
    isoStr = rGetIsoDate(now);
    parsed = rParseIsoDate(isoStr);
    teqll(parsed, now);

    // Error cases - should return -1
    parsed = rParseIsoDate(NULL);
    teqll(parsed, -1);

    parsed = rParseIsoDate("");
    teqll(parsed, -1);

    parsed = rParseIsoDate("invalid");
    teqll(parsed, -1);

    parsed = rParseIsoDate("2023-12-25");
    teqll(parsed, -1);

    parsed = rParseIsoDate("2023-12-25T10:30:45");
    teqll(parsed, -1);

    parsed = rParseIsoDate("2023-12-25T10:30:45.123");
    teqll(parsed, -1);

    parsed = rParseIsoDate("2023-12-25T10:30:45Z extra");
    teqll(parsed, -1);

    parsed = rParseIsoDate("2023-12-25T10:30:45+99:00");
    teqll(parsed, -1);

    parsed = rParseIsoDate("2023-12-25T10:30:45+00:99");
    teqll(parsed, -1);

    parsed = rParseIsoDate("2023-13-25T10:30:45Z");
    teqll(parsed, -1);

    parsed = rParseIsoDate("2023-12-32T10:30:45Z");
    teqll(parsed, -1);

    parsed = rParseIsoDate("2023-12-25T25:30:45Z");
    teqll(parsed, -1);

    parsed = rParseIsoDate("2023-12-25T10:60:45Z");
    teqll(parsed, -1);

    parsed = rParseIsoDate("2023-12-25T10:30:61Z");
    teqll(parsed, -1);
}

static void testHiResTicks()
{
    uint64 ticks1, ticks2;

    ticks1 = rGetHiResTicks();
    tgtll(ticks1, 0);

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

/*
    The rejections in the date parsers and the two accessors nothing called.
 */
static void timeCorners()
{
    struct tm tm;
    Time      when, mark;
    char      *cp;

    //  A year outside the parser's range, and a timezone offset that is not two digits
    ttrue(rParseIsoDate("0001-01-01T00:00:00Z") < 0);
    ttrue(rParseIsoDate("2024-01-01T00:00:00+0X:00") < 0);
    ttrue(rParseIsoDate("2024-01-01T00:00:00+05:30") > 0);

    cp = rGetHttpDate(0);
    tmatch(cp, "Thu, 01 Jan 1970 00:00:00 GMT");
    rFree(cp);

    //  Elapsed time is never negative and a made time round-trips
    mark = rGetTime();
    ttrue(rGetElapsedTime(mark) >= 0);

    memset(&tm, 0, sizeof(tm));
    tm.tm_year = 100;
    tm.tm_mon = 0;
    tm.tm_mday = 1;
    tm.tm_isdst = -1;
    when = rMakeTime(&tm);
    ttrue(when > 0);

    //  A deadline already in the past has nothing remaining
    ttrue(rGetRemainingTicks(rGetTicks() - 5000, 1000) <= 0);
}


static void testHttpDate(void)
{
    static cchar *invalid[] = {
        "Xxx, 06 Nov 1994 08:49:37 GMT",       //  Not a weekday
        "Sun 06 Nov 1994 08:49:37 GMT",        //  No comma
        "Sun, 06 Foo 1994 08:49:37 GMT",       //  Not a month
        "Sun, 32 Nov 1994 08:49:37 GMT",       //  Day out of range
        "Sun, 00 Nov 1994 08:49:37 GMT",
        "Sun, 31 Nov 1994 08:49:37 GMT",       //  November has 30 days
        "Wed, 29 Feb 2023 08:49:37 GMT",       //  Not a leap year
        "Mon, 29 Feb 2100 08:49:37 GMT",       //  Century years are not leap years unless divisible by 400
        "Sun, 06 Nov 1994 24:49:37 GMT",       //  Hour out of range
        "Sun, 06 Nov 1994 08:60:37 GMT",
        "Sun, 06 Nov 1994 08:49:61 GMT",
        "Wed, 31 Dec 1969 23:59:59 GMT",       //  Before the epoch
        "Sun, 06 Nov 94 08:49:37 GMT",         //  Two digit year
        "Sun, 06 Nov 19944 08:49:37 GMT",
        "Sun, 06 Nov 1994 08:49:37",           //  No zone
        "Sun, 06 Nov 1994 08:49:37 UTC",
        "Sun, 06 Nov 1994 08:49:37 gmt",
        "Sun, 06 Nov 1994 08:49:37 GMT junk",  //  Trailing text
        "Sun, 06 Nov 1994 08:49",
        "Sun, 06 Nov 1994",
        "Sunday, 06-Nov-94 08:49:37 GMT",      //  The obsolete RFC 850 form
        "Sun Nov  6 08:49:37 1994",            //  The obsolete asctime form
        " Sun, 06 Nov 1994 08:49:37 GMT",
        "Sun,",
        "Sun",
        "Su",
    };
    cchar *tz;
    char  *date, *oldTz;
    Time  when, parsed;
    int   i;

    //  The examples of RFC 9110
    teqz(rParseHttpDate("Sun, 06 Nov 1994 08:49:37 GMT"), 784111777000LL);
    teqz(rParseHttpDate("Thu, 01 Jan 1970 00:00:01 GMT"), 1000);
    teqz(rParseHttpDate("Thu, 29 Feb 2024 12:00:00 GMT"), 1709208000000LL);
    teqz(rParseHttpDate("Tue, 29 Feb 2000 00:00:00 GMT"), 951782400000LL);
    teqz(rParseHttpDate("Sun, 31 Dec 2023 23:59:59 GMT"), 1704067199000LL);
    teqz(rParseHttpDate("Fri, 31 Dec 9999 23:59:59 GMT"), 253402300799000LL);

    //  A single digit day, and the parameter that old clients add after the date
    teqz(rParseHttpDate("Sun, 6 Nov 1994 08:49:37 GMT"), 784111777000LL);
    teqz(rParseHttpDate("Sun, 06 Nov 1994 08:49:37 GMT; length=1234"), 784111777000LL);

    //  A leap second carries into the next minute
    teqz(rParseHttpDate("Sat, 31 Dec 2016 23:59:60 GMT"), 1483228800000LL);

    //  The epoch itself is the same as an invalid date: zero
    teqz(rParseHttpDate("Thu, 01 Jan 1970 00:00:00 GMT"), 0);

    for (i = 0; i < (int) (sizeof(invalid) / sizeof(*invalid)); i++) {
        parsed = rParseHttpDate(invalid[i]);
        teqz(parsed, 0, "Must be refused: %s", invalid[i]);
    }

    //  Every date the formatter can produce parses back to the same second
    for (when = 0; when < 4102444800000LL; when += 86400000LL * 37 + 12345000LL) {
        date = rGetHttpDate(when);
        tnotnull(date);
        parsed = rParseHttpDate(date);
        teqz(parsed, when, "Round trip of %s", date);
        rFree(date);
    }

#if ME_UNIX_LIKE
    //  The result does not depend on the local time zone
    tz = getenv("TZ");
    oldTz = tz ? sclone(tz) : NULL;
    setenv("TZ", "Australia/Sydney", 1);
    tzset();
    teqz(rParseHttpDate("Sun, 06 Nov 1994 08:49:37 GMT"), 784111777000LL);
    setenv("TZ", "America/New_York", 1);
    tzset();
    teqz(rParseHttpDate("Sun, 06 Nov 1994 08:49:37 GMT"), 784111777000LL);
    if (oldTz) {
        setenv("TZ", oldTz, 1);
    } else {
        unsetenv("TZ");
    }
    tzset();
    rFree(oldTz);
#else
    (void) tz;
    (void) oldTz;
    tinfo("Skipped: The time zone is not changed on this platform");
#endif
}



static void testUnrepresentableTime(void)
{
    char *iso, *utc, *local;
    Time when;

    when = 5270054400000LL;
    iso = rGetIsoDate(when);
    utc = rFormatUniversalTime("%F", when);
    local = rFormatLocalTime("%Y", when + 86400 * TPS);
    tmatch(iso, "2137-01-01T00:00:00.000Z");
    tmatch(utc, "2137-01-01");
    if (sizeof(time_t) < sizeof(Time)) {
        tnull(local);
    } else {
        tmatch(local, "2137");
    }
    rFree(iso);
    rFree(utc);
    rFree(local);
}

static void testUniversalRange(void)
{
    char *cp;
    Time when;

    //  The last instant strftime accepts everywhere. The Windows runtime alone stops at the year 3000.
    cp = rGetIsoDate(253402300799999LL);
    tmatch(cp, "9999-12-31T23:59:59.999Z");
    rFree(cp);
    when = rParseIsoDate("9999-12-31T23:59:59.999Z");
    teqll(when, 253402300799999LL);

    cp = rGetHttpDate(253402300799999LL);
    tmatch(cp, "Fri, 31 Dec 9999 23:59:59 GMT");
    rFree(cp);

    cp = rFormatUniversalTime("%Y-%m-%d %H:%M:%S %j", 32503680000000LL);
    tmatch(cp, "3000-01-01 00:00:00 001");
    rFree(cp);

    cp = rFormatUniversalTime("%Y-%m-%d %j", 951782400000LL);
    tmatch(cp, "2000-02-29 060");
    rFree(cp);

    //  Before the epoch
    when = rParseIsoDate("1900-01-01T00:00:00Z");
    teqll(when, -2208988800000LL);
    cp = rGetHttpDate(when);
    tmatch(cp, "Mon, 01 Jan 1900 00:00:00 GMT");
    rFree(cp);

    when = rParseIsoDate("1969-12-31T23:59:59Z");
    teqll(when, -1000LL);
    cp = rGetHttpDate(when);
    tmatch(cp, "Wed, 31 Dec 1969 23:59:59 GMT");
    rFree(cp);

    //  Past the year 9999
    cp = rGetIsoDate(253402300800000LL);
    tnull(cp);
    cp = rGetHttpDate(MAXINT64);
    tnull(cp);
}

static void testMakeUniversalTime(void)
{
    struct tm tm;
    Time      when;

    memset(&tm, 0, sizeof(tm));
    tm.tm_year = 8099;
    tm.tm_mon = 11;
    tm.tm_mday = 31;
    tm.tm_hour = 23;
    tm.tm_min = 59;
    tm.tm_sec = 59;
    when = rMakeUniversalTime(&tm);
    teqll(when, 253402300799LL);

    //  Out of range fields carry over
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = 100;
    tm.tm_mon = 13;
    tm.tm_mday = 0;
    when = rMakeUniversalTime(&tm);
    teqll(when, rParseIsoDate("2001-01-31T00:00:00Z") / TPS);

    memset(&tm, 0, sizeof(tm));
    tm.tm_year = 70;
    tm.tm_mon = -1;
    tm.tm_mday = 1;
    when = rMakeUniversalTime(&tm);
    teqll(when, -31 * 86400LL);
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
    timeCorners();
    testHttpDate();
    testUnrepresentableTime();
    testUniversalRange();
    testMakeUniversalTime();
    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
