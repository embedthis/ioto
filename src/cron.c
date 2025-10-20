/*
   cron.c -- cron management

    The cron module provides routines to test if cron specs are due to
    be run and the time to wait till they can be run.

    Note:  cron specs use the following ranges

        Minutes			0-59
        Hours			0-23
        Days			1-31
        Months			1-12
        Day of week		0-6 (sunday is 0)
 */

/********************************* Includes ***********************************/

#include    "ioto.h"

#if SERVICES_CRON
/********************************* Defines ************************************/

#define MINUTE  60L
#define HOUR    (60L * 60L)
#define DAY     (24L * 60L * 60L)
#define INVALID 100

typedef struct Cron {
    char *minute;
    char *hour;
    char *day;
    char *month;
    cchar *dayofweek;
} Cron;

static int perMonth[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

/*************************** Forward Declarations *****************************/

static int nextValue(int current, cchar *str);
static int atoia(cchar **ptr);
static int64 between(int m1, int d1, int y1, int m2, int d2, int y2);
static int daysPerMonth(int m, int y);
static bool cronMatch(Cron *cp, struct tm *tm);

/*********************************** Code *************************************/
/*
    Convert a string to a cron spec. Return -1 if any part is null. This is a
    rudimentary check to see if the cron spec is OK. It could (should?) be improved.
 */
static Cron *cronAlloc(cchar *spec)
{
    Cron *cp;
    char *buf, *rest;

    if (spec == NULL || *spec == '\0') {
        spec = "* * * * *";
    }
    cp = rAllocType(Cron);

    /*
        Convenient aliases
     */
    if (smatch(spec, "anytime")) {
        spec = "* * * * *";
    } else if (smatch(spec, "never") || smatch(spec, "unscheduled")) {
        spec = "0 0 0 0 0";
    } else if (smatch(spec, "day")) {
        spec = "* 6-17 * * *";
    } else if (smatch(spec, "weekdays")) {
        spec = "* * * * 1-5";
    } else if (smatch(spec, "workhours")) {
        spec = "* 9-17 * * 1-5";
    } else if (smatch(spec, "midnight")) {
        spec = "* 0 * * *";
    } else if (smatch(spec, "night")) {
        spec = "* 0-5,18-23 * * *";
    }
    buf = sclone(spec);

    /*
        cp->minute holds the values
     */
    cp->minute = stok(buf, " ", &rest);
    cp->hour = stok(rest, " ", &rest);
    cp->day = stok(rest, " ", &rest);
    cp->month = stok(rest, " ", &rest);
    cp->dayofweek = stok(rest, " ", &rest);

    if (cp->minute == NULL || cp->hour == NULL || cp->day == NULL ||
        cp->month == NULL || cp->dayofweek == NULL) {
        rFree(buf);
        rFree(cp);
        return 0;
    }
    return cp;
}

static void cronFree(Cron *cs)
{
    assert(cs);
    /*
        The other elements point into the minute array, so we only need to free it
     */
    if (cs->minute) {
        rFree(cs->minute);
    }
    rFree(cs);
}

/*
    Return the time in ticks to wait till the next valid time to run a cron entry.
 */
Ticks cronUntil(cchar *spec, Time when)
{
    Cron      *cp;
    struct tm *tm;
    Time      now;
    time_t    t;
    int64     days, db;
    int       tm_mon, tm_mday, tm_wday, wday, m, min, h, hr, carry, day,
              d1, day1, carry1, d2, day2, carry2, daysahead, mon,
              yr, wd, today;

    assert(spec);

    if ((cp = cronAlloc(spec)) == NULL) {
        return -1;
    }
    if (smatch(cp->month, "0")) {
        // Never
        cronFree(cp);
        return MAXTIME;
    }
    now = rGetTime() / TPS;
    if (when == 0) {
        when = now * TPS;
    }
    t = (time_t) when / TPS;
    tm = localtime(&t);

    tm_mon = nextValue(tm->tm_mon + 1, cp->month) - 1;
    tm_mday = nextValue(tm->tm_mday, cp->day);
    tm_wday = nextValue(tm->tm_wday, cp->dayofweek);
    today = 1;

    if ((scmp(cp->day, "*") == 0 && tm->tm_wday != tm_wday)
        || (scmp(cp->dayofweek, "*") == 0 && tm->tm_mday != tm_mday)
        || (tm->tm_mday != tm_mday && tm->tm_wday != tm_wday)
        || (tm->tm_mon != tm_mon)) {
        today = 0;
    }
    m = tm->tm_min;
    if ((tm->tm_hour + 1) <= nextValue(tm->tm_hour % 24, cp->hour)) {
        m = 0;
    }
    min = nextValue(m % 60, cp->minute);
    carry = (min < m) ? 1 : 0;
    h = tm->tm_hour + carry;
    hr = nextValue(h % 24, cp->hour);
    carry = (hr < h) ? 1:0;

    /*
        Today's events
     */
    if (!carry && today) {
        if (tm->tm_min > min) {
            t += (Ticks) (hr - tm->tm_hour - 1) * HOUR + (Ticks) (60 - tm->tm_min + min) * MINUTE;
        } else {
            t += (Ticks) (hr - tm->tm_hour) * HOUR + (Ticks) (min - tm->tm_min) * MINUTE;
        }
        t -= (long) tm->tm_sec + now;
        if (t < 0) {
            t = 0;
        }
        cronFree(cp);
        return (Ticks) t * TPS;
    }

    min = nextValue(0, cp->minute);
    hr = nextValue(0, cp->hour);

    /*
        Get the due date of this event
     */
    d1 = tm->tm_mday + 1;
    days = daysPerMonth(tm->tm_mon, tm->tm_year);
    day1 = nextValue((d1 - 1) % days + 1, cp->day);
    carry1 = (day1 < d1) ? 1 : 0;

    d2 = tm->tm_wday + 1;
    wday = nextValue(d2 % 7, cp->dayofweek);
    if (wday < d2) {
        daysahead = 7 - d2 + wday;
    } else {
        daysahead = wday - d2;
    }
    day2 = (d1 + daysahead - 1) % days + 1;
    carry2 = (day2 < d1) ? 1:0;

    /*
        Work out whether to use the day of month or day of week specs
     */
    if ((scmp(cp->day, "*") == 0) && (scmp(cp->dayofweek, "*") != 0)) {
        day1 = day2;
        carry1 = carry2;
    }
    if ((scmp(cp->day, "*") != 0) && (scmp(cp->dayofweek, "*") == 0)) {
        day2 = day1;
        carry2 = carry1;
    }
    yr = tm->tm_year;
    if ((carry1 && carry2) || (tm->tm_mon != tm_mon)) {
        /*
            Events that don't occur this month
         */
        m = tm->tm_mon + 1;
        mon = nextValue(m % 12 + 1, cp->month) - 1;
        carry = (mon < m) ? 1 : 0;
        yr += carry;
        day1 = nextValue(1, cp->day);
        db = between(tm->tm_mon, tm->tm_mday, tm->tm_year, mon, 1, yr) + 1;
        wd = (tm->tm_wday + db) % 7;
        wday = nextValue(wd, cp->dayofweek);
        if (wday < wd) {
            day2 = 1 + 7 - wd + wday;
        } else {
            day2 = 1 + wday - wd;
        }
        if ((scmp(cp->day, "*") != 0) &&
            (scmp(cp->dayofweek, "*") == 0)) {
            day2 = day1;
        }
        if ((scmp(cp->day, "*") == 0) &&
            (scmp(cp->dayofweek, "*") != 0)) {
            day1 = day2;
        }
        day = (day1 < day2) ? day1 : day2;

    } else {
        /*
            Events that occur this month
         */
        mon = tm->tm_mon;
        if (!carry1 && !carry2) {
            day = (day1 < day2) ? day1 : day2;
        } else if (!carry1) {
            day = day1;
        } else {
            day = day2;
        }
    }
    days = between(tm->tm_mon, tm->tm_mday, tm->tm_year, mon, day, yr);

    t += (Ticks) (23 - tm->tm_hour) * HOUR + (Ticks) (60 - tm->tm_min) * MINUTE
         + (Ticks) hr * HOUR + (Ticks) min * MINUTE + (Ticks) days * DAY;

    t -= (long) tm->tm_sec + now;
    if (t < 0) {
        t = 0;
    }
    cronFree(cp);
    return (Ticks) t * TPS;
}

/**
    Returns the time remaining until the end of the current window.
 */
PUBLIC Ticks cronUntilEnd(cchar *spec, Time when)
{
    Cron      *cp;
    struct tm *tm;              /* Current time broken down */
    struct tm end_tm;           /* End of cron window broken down */
    time_t    t;                /* Current time in seconds */
    time_t    end_t;            /* End of cron window in seconds */

    if ((cp = cronAlloc(spec)) == NULL) {
        return -1;
    }
    if (when == 0) {
        when = rGetTime();
    }
    t = (time_t) when / TPS;
    tm = localtime(&t);

    /*
        Return 0 if the cron spec is not currently active.
     */
    if (!cronMatch(cp, tm)) {
        cronFree(cp);
        return 0;
    }
    end_tm = *tm;

    /*
        Calculate the end of the current window based on the most specific cron field.
     */
    if (scmp(cp->minute, "*") != 0) {
        /* End of the current minute */
        end_tm.tm_sec = 59;

    } else if (scmp(cp->hour, "*") != 0) {
        /* End of the current hour */
        end_tm.tm_min = 59;
        end_tm.tm_sec = 59;

    } else if (scmp(cp->day, "*") != 0 || scmp(cp->dayofweek, "*") != 0) {
        /* End of the current day */
        end_tm.tm_hour = 23;
        end_tm.tm_min = 59;
        end_tm.tm_sec = 59;

    } else if (scmp(cp->month, "*") != 0) {
        /* End of the current month */
        end_tm.tm_mday = daysPerMonth(tm->tm_mon, tm->tm_year + 1900);
        end_tm.tm_hour = 23;
        end_tm.tm_min = 59;
        end_tm.tm_sec = 59;

    } else {
        /* All fields are "*", so the window is indefinite */
        cronFree(cp);
        return MAXINT64 - MAXINT;
    }
    cronFree(cp);
    end_t = mktime(&end_tm);
    if (end_t < t) {
        return 0;
    }
    return (Ticks) (end_t - t) * TPS;
}

/*
    Return true if the given time matches the cron spec
 */
static bool cronMatch(Cron *cp, struct tm *tm)
{
    bool dayMatch, dowMatch;

    if (nextValue(tm->tm_min, cp->minute) != tm->tm_min) return 0;
    if (nextValue(tm->tm_hour, cp->hour) != tm->tm_hour) return 0;
    if (nextValue(tm->tm_mon + 1, cp->month) != tm->tm_mon + 1) return 0;

    dayMatch = (nextValue(tm->tm_mday, cp->day) == tm->tm_mday);
    dowMatch = (nextValue(tm->tm_wday, cp->dayofweek) == tm->tm_wday);

    if (smatch(cp->day, "*")) {
        return dowMatch;
    }
    if (smatch(cp->dayofweek, "*")) {
        return dayMatch;
    }
    return dayMatch || dowMatch;
}

/*
    Return the next valid value for a particular cron field that is greater or equal
    to the current field.  If no valid values exist the smallest of the list is returned.
 */
static int nextValue(int current, cchar *str)
{
    cchar *ptr;
    int   n, n2, min, min_gt;

    if (scmp(str, "*") == 0) {
        return current;
    }
    ptr = str;
    min = INVALID;
    min_gt = INVALID;
    while (1) {
        if ((n = atoia(&ptr)) == current) {
            return current;
        }
        if (n < min) {
            min = n;
        }
        if ((n > current) && (n < min_gt)) {
            min_gt = n;
        }
        if (*ptr == '-') {
            ptr++;
            if ((n2 = atoia(&ptr)) > n) {
                if ((current > n) && (current <= n2)) {
                    return current;
                }
            } else {
                if (current > n || current <= n2) {
                    return current;
                }
            }
        }
        if (*ptr == '\0') {
            break;
        }
        ptr += 1;
    }
    if (min_gt != INVALID) {
        return min_gt;
    }else return min;
}

/*
    ascii to integer with pointer advance
 */
static int atoia(cchar **ptr)
{
    Ticks n;
    int   digit;

    n = 0;
    while (isdigit((int) **ptr)) {
        digit = **ptr - '0';
        // SECURITY Acceptable: Integer overflow protection with intentional division truncation
        if (n > (LLONG_MAX - digit) / 10) {
            n = LLONG_MAX;
            while (isdigit((int) **ptr)) {
                *ptr += 1;
            }
            break;
        }
        n = n * 10 + digit;
        *ptr += 1;
    }
    if (n > INT_MAX) {
        return INT_MAX;
    }
    return (int) n;
}

/*
    Return the number of complete days between two dates
 */
static int64 between(int m1, int d1, int y1, int m2, int d2, int y2)
{
    int days, m;

    if (m1 < 0 || m2 < 0) {
        return 0;
    }
    if ((m1 == m2) && (d1 == d2) && (y1 == y2)) {
        return 0;
    }
    if ((m1 == m2) && (d1 < d2)) {
        return d2 - d1 - 1;
    }
    /*
        These are not in the same month
     */
    days = (daysPerMonth(m1, y1) - d1) + (d2 - 1);
    m = (m1 + 1) % 12;
    while (m != m2) {
        if (m == 0)
            y1++;
        days += daysPerMonth(m, y1);
        m = (m + 1) % 12;
    }
    return days;
}

/*
    Return the number of days in a month

    Leap year rule: a year is a leap year if it is divisible by 4, but not
    by 100, except that years divisible by 400 ARE leap years.
 */
static int daysPerMonth(int m, int y)
{
    return perMonth[m] + (((m == 1) && ((y % 4) == 0) && ((y % 100) != 0)) ||
                          ((m == 1) && (y % 400 == 0)) ? 1 : 0);
}

#else
PUBLIC void dummyCron(void)
{
}
#endif /* SERVICES_CRON */