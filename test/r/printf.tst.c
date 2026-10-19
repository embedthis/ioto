/*
    printf.tst.c - Unit tests for the Sprintf class

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/
/*
    We support some non-standard PRINTF args
 */
//#define PRINTF_ATTRIBUTE(a,b)

#include    "testme.h"
#include    "r.h"
#include    <math.h>

/************************************ Code ************************************/

static void basicSprintf()
{
    char  buf[256];
    char  *str;
    ssize len;
    int   count;

    sfmtbuf(buf, sizeof(buf), "%d", 12345678);
    teqz(slen(buf), 8);
    tmatch(buf, "12345678");

    sfmtbuf(buf, sizeof(buf), "%d", -12345678);
    teqz(slen(buf), 9);
    tmatch(buf, "-12345678");

    str = sfmt("%d", 12345678);
    count = (int) slen(str);
    teqi(count, 8);
    tmatch(str, "12345678");

    sfmtbuf(buf, sizeof(buf), "%f", -37.1234);
    tmatch(buf, "-37.123400");

    sfmtbuf(buf, sizeof(buf), "%g", -37.1234);
    tmatch(buf, "-37.1234");

    sfmtbuf(buf, sizeof(buf), "%e", -37.1234);
    tmatch(buf, "-3.712340e+01");

    len = rSnprintf(buf, sizeof(buf), "%g", 12.6);
    teqz(len, 4);
    tmatch(buf, "12.6");
}

static void itostring()
{
    char *s;

    s = sitos(0);
    tmatch(s, "0");

    s = sitos(1);
    tmatch(s, "1");

    s = sitos(-1);
    tmatch(s, "-1");

    s = sitos(12345678);
    tmatch(s, "12345678");

    s = sitos(-12345678);
    tmatch(s, "-12345678");

    s = sitosx(0x1234, 16);
    tmatch(s, "1234");
}

/*
    We need to test quite a bit here. The general format of a sprintf spec is:

        %[modifier][width][precision][bits][type]

    The various character classes are:
        CLASS       Characters      Description
        NORMAL      [All other]     Normal characters
        PERCENT     [%]             Begin format
        MODIFIER    [-+ #,]         Modifiers
        ZERO        [0]             Special modifier
        STAR        [*]             Width supplied by arg
        DIGIT       [1-9]           Field widths
        DOT         [.]             Introduce precision
        BITS        [hlL]           Length bits
        TYPE        [cdfinopsSuxX]  Type specifiers
 */
static void typeOptions()
{
    char buf[256];

    sfmtbuf(buf, sizeof(buf), "Hello %c World", 'X');
    tmatch(buf, "Hello X World");

    sfmtbuf(buf, sizeof(buf), "%d", 12345678);
    tmatch(buf, "12345678");

    sfmtbuf(buf, sizeof(buf), "%lf", 44444444440.0);
    tmatch(buf, "44444444440.000000");

    sfmtbuf(buf, sizeof(buf), "%i", 12345678);
    sfmtbuf(buf, sizeof(buf), "%3.2f", 1.77);
    tmatch(buf, "1.77");

    sfmtbuf(buf, sizeof(buf), "%i", 12345678);
    tmatch(buf, "12345678");

    sfmtbuf(buf, sizeof(buf), "%o", 077);
    tmatch(buf, "77");

    sfmtbuf(buf, sizeof(buf), "%p", (void*) (ssize) 0xdeadbeef);
    tmatch(buf, "0xdeadbeef");

    sfmtbuf(buf, sizeof(buf), "%s", "Hello World");
    tmatch(buf, "Hello World");

    sfmtbuf(buf, sizeof(buf), "%u", 0xffffffff);
    tmatch(buf, "4294967295");

    sfmtbuf(buf, sizeof(buf), "%x", 0xffffffff);
    tmatch(buf, "ffffffff");

    sfmtbuf(buf, sizeof(buf), "%llX", (int64) 0xffffffff);
    tmatch(buf, "FFFFFFFF");
}

static void floatValues()
{
    char buf[256];

    sfmtbuf(buf, sizeof(buf), "%g", 1234.5678);
    tmatch(buf, "1234.5678");

    sfmtbuf(buf, sizeof(buf), "%g", -37.1234);
    tmatch(buf, "-37.1234");

    sfmtbuf(buf, sizeof(buf), "%f", -37.1234);
    tmatch(buf, "-37.123400");

    sfmtbuf(buf, sizeof(buf), "%g", -37.0);
    tmatch(buf, "-37");

    sfmtbuf(buf, sizeof(buf), "%f", -37.0);
    tmatch(buf, "-37.000000");

    sfmtbuf(buf, sizeof(buf), "%e", -37.1234);
    tmatch(buf, "-3.712340e+01");

    sfmtbuf(buf, sizeof(buf), "%E", -37.1234);
    tmatch(buf, "-3.712340E+01");

    sfmtbuf(buf, sizeof(buf), "%e", 0.0001234);
    tmatch(buf, "1.234000e-04");

    sfmtbuf(buf, sizeof(buf), "%e", 1000000.1234);
    tmatch(buf, "1.000000e+06");

    sfmtbuf(buf, sizeof(buf), "%.2e", 1000000.1234);
    tmatch(buf, "1.00e+06");
}

static void modifierOptions()
{
    char buf[256];

    sfmtbuf(buf, sizeof(buf), "%-4d", 23);
    tmatch(buf, "23  ");
    sfmtbuf(buf, sizeof(buf), "%-4d", -23);
    tmatch(buf, "-23 ");

    sfmtbuf(buf, sizeof(buf), "%+4d", 23);
    tmatch(buf, " +23");
    sfmtbuf(buf, sizeof(buf), "%+4d", -23);
    tmatch(buf, " -23");

    sfmtbuf(buf, sizeof(buf), "% 4d", 23);
    tmatch(buf, "  23");
    sfmtbuf(buf, sizeof(buf), "% 4d", -23);
    tmatch(buf, " -23");

    sfmtbuf(buf, sizeof(buf), "%-+4d", 23);
    tmatch(buf, "+23 ");
    sfmtbuf(buf, sizeof(buf), "%-+4d", -23);
    tmatch(buf, "-23 ");
    sfmtbuf(buf, sizeof(buf), "%- 4d", 23);
    tmatch(buf, " 23 ");

    sfmtbuf(buf, sizeof(buf), "%#6x", 0x23);
    tmatch(buf, "  0x23");

    sfmtbuf(buf, sizeof(buf), "%+03d", 7);
    tmatch(buf, "+07");

    sfmtbuf(buf, sizeof(buf), "%+03d", -7);
    tmatch(buf, "-07");
}

static void widthOptions()
{
    char buf[256];

    sfmtbuf(buf, sizeof(buf), "%2d", 1234);
    tmatch(buf, "1234");

    sfmtbuf(buf, sizeof(buf), "%8d", 1234);
    tmatch(buf, "    1234");

    sfmtbuf(buf, sizeof(buf), "%-8d", 1234);
    tmatch(buf, "1234    ");

    sfmtbuf(buf, sizeof(buf), "%*d", 8, 1234);
    tmatch(buf, "    1234");

    sfmtbuf(buf, sizeof(buf), "%*d", -8, 1234);
    tmatch(buf, "1234    ");
}

static void precisionOptions()
{
    char buf[256];

    sfmtbuf(buf, sizeof(buf), "%.2d", 1234);
    tmatch(buf, "1234");

    sfmtbuf(buf, sizeof(buf), "%.8d", 1234);
    tmatch(buf, "00001234");

    sfmtbuf(buf, sizeof(buf), "%8.6d", 1234);
    tmatch(buf, "  001234");

    sfmtbuf(buf, sizeof(buf), "%6.3d", 12345);
    tmatch(buf, " 12345");

    sfmtbuf(buf, sizeof(buf), "%6.3s", "ABCDEFGHIJ");
    tmatch(buf, "   ABC");

    sfmtbuf(buf, sizeof(buf), "%6.2f", 12.789);
    tmatch(buf, " 12.79");

    sfmtbuf(buf, sizeof(buf), "%8.2f", 1234.789);
    tmatch(buf, " 1234.79");

    sfmtbuf(buf, sizeof(buf), "%.5f", -37.814);
    tmatch(buf, "-37.81400");
}

static void bitOptions()
{
    char buf[256];

    sfmtbuf(buf, sizeof(buf), "%hd %hd", (short) 23, (short) 78);
    tmatch(buf, "23 78");

    sfmtbuf(buf, sizeof(buf), "%ld %ld", (long) 12, (long) 89);
    tmatch(buf, "12 89");

    sfmtbuf(buf, sizeof(buf), "%lld %lld", (int64) 66, (int64) 41);
    tmatch(buf, "66 41");

    sfmtbuf(buf, sizeof(buf), "%hd %lld %hd %lld",
            (short) 123, (int64) 789, (short) 441, (int64) 558);
    tmatch(buf, "123 789 441 558");
}


static void sprintf64()
{
    char buf[256];

    sfmtbuf(buf, sizeof(buf), "%lld", INT64(9012345678));
    teqz(slen(buf), 10);
    tmatch(buf, "9012345678");

    sfmtbuf(buf, sizeof(buf), "%lld", INT64(-9012345678));
    teqz(slen(buf), 11);
    tmatch(buf, "-9012345678");
}

static void overflow()
{
    char buf[256];

    sfmtbuf(buf, sizeof(buf), "%lld", INT64(9012345678));
    teqz(slen(buf), 10);
    tmatch(buf, "9012345678");

    int64 minValue = INT64_MIN;
    rSnprintf(buf, sizeof(buf), "%lld", minValue);
    tmatch(buf, "-9223372036854775808");
}

static void extra()
{
    char buf[256];

    sfmtbuf(buf, sizeof(buf), "%e", 12345.6789);
    tmatch(buf, "1.234568e+04");

    // Test case 2: Positive value, uppercase (%E)
    sfmtbuf(buf, sizeof(buf), "%E", 12345.6789);
    tmatch(buf, "1.234568E+04");

    // Test case 3: Negative value, default precision (%e)
    sfmtbuf(buf, sizeof(buf), "%e", -12345.6789);
    tmatch(buf, "-1.234568e+04");

    // Test case 4: Zero value
    sfmtbuf(buf, sizeof(buf), "%e", 0.0);
    tmatch(buf, "0.000000e+00");

    // Test case 5: Specific precision (.2e) with rounding down
    sfmtbuf(buf, sizeof(buf), "%.2e", 12345.6789);
    tmatch(buf, "1.23e+04");

    // Test case 6: Specific precision (.2e) with rounding up
    sfmtbuf(buf, sizeof(buf), "%.2e", 1.235);
    tmatch(buf, "1.24e+00");

    // Test case 7: Zero precision (.0e)
    sfmtbuf(buf, sizeof(buf), "%.0e", 12345.6789);
    tmatch(buf, "1e+04");

    // Test case 8: Zero precision (.0E) with rounding up
    sfmtbuf(buf, sizeof(buf), "%.0E", 17345.6789);
    tmatch(buf, "2E+04");

    // Test case 9: Rounding up that changes the exponent
    sfmtbuf(buf, sizeof(buf), "%e", 9.9999999);
    tmatch(buf, "1.000000e+01");

    // Test case 10: High precision (.10e)
    sfmtbuf(buf, sizeof(buf), "%.10e", 1.234567890123);
    tmatch(buf, "1.2345678901e+00");

    // Test case 11: Exponent with more than 2 digits (will likely fail)
    sfmtbuf(buf, sizeof(buf), "%e", 1.23e123);
    tmatch(buf, "1.230000e+123");
}

static void regress()
{
    char buf[256];

    sfmtbuf(buf, sizeof(buf), "%f", 2.0);
    tmatch(buf, "2.000000");

    sfmtbuf(buf, sizeof(buf), "%g", 2.0);
    tmatch(buf, "2");

    sfmtbuf(buf, sizeof(buf), "%g", 20.0);
    tmatch(buf, "20");

    sfmtbuf(buf, sizeof(buf), "%g, after", 20.0);
    tmatch(buf, "20, after");
}

/*
    The output entry points. rPrintf writes to descriptor 1 and rFprintf to a stream; both are
    pointed at a temporary file so a passing run stays quiet and the bytes can be read back.

    The TestMe comparison macros expand their arguments twice, so every call with a side effect
    is bound to a local before it is asserted.
 */
static void printfOutput()
{
    FILE  *fp;
    char  path[ME_MAX_FNAME];
    char  *contents;
    ssize len;
    int   saved, fd;

    sfmtbuf(path, sizeof(path), "data/temp-printf-%d.out", (int) getpid());
    unlink(path);

    /*
        rFprintf takes the stream, so no redirection is needed.
     */
    fp = fopen(path, "w");
    tnotnull(fp);
    len = rFprintf(fp, "value %d and %s\n", 42, "text");
    teqz(len, 18);
    fclose(fp);

    contents = rReadFile(path, NULL);
    tnotnull(contents);
    tmatch(contents, "value 42 and text\n");
    rFree(contents);

    len = rFprintf(NULL, "%s", "nowhere");
    teqz(len, R_ERR_BAD_HANDLE);

    /*
        rPrintf writes to descriptor 1, so redirect it around the call and put it back.
     */
    unlink(path);
    saved = dup(1);
    ttrue(saved >= 0);
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    ttrue(fd >= 0);
    dup2(fd, 1);
    close(fd);

    len = rPrintf("printed %d\n", 7);

    fsync(1);
    dup2(saved, 1);
    close(saved);

    teqz(len, 10);
    contents = rReadFile(path, NULL);
    tnotnull(contents);
    tmatch(contents, "printed 7\n");
    rFree(contents);

    unlink(path);
}


/*
    The buffer growth path inside the formatter. A dynamically allocated result grows repeatedly;
    a caller-supplied buffer cannot grow and truncates instead; a maxsize caps the growth.
 */
static ssize saprintf(char **buf, size_t maxsize, cchar *fmt, ...)
{
    va_list args;
    ssize   len;

    va_start(args, fmt);
    len = rVsaprintf(buf, maxsize, fmt, args);
    va_end(args);
    return len;
}


static void printfGrowth()
{
    char  tiny[8];
    char  *buf;
    ssize len;

    /*
        Far larger than the formatter's initial allocation, so the growth loop runs many times.
     */
    buf = NULL;
    len = saprintf(&buf, 0, "%s", "0123456789");
    teqz(len, 10);
    rFree(buf);

    /*
        Exceed the initial buffer size while staying below the width cap.
     */
    buf = sfmt("%0*d", 4000, 1);
    tnotnull(buf);
    teqz(slen(buf), 4000);
    rFree(buf);

    /*
        A caller-supplied buffer cannot grow: the result is truncated and still terminated.
     */
    len = rSnprintf(tiny, sizeof(tiny), "%s", "0123456789abcdef");
    teqz(len, 16);
    teqi(tiny[sizeof(tiny) - 1], 0);
    tmatch(tiny, "0123456");

    /*
        maxsize caps a dynamic allocation. A result that would exceed it is not truncated and
        returned: the buffer is freed and the caller gets an error with a null pointer, so a
        caller that ignores the return has nothing to free and nothing to read.
     */
    buf = NULL;
    len = saprintf(&buf, 16, "%0*d", 20000, 1);
    teqz(len, R_ERR_MEMORY);
    tnull(buf);

    /*
        A result that fits inside maxsize is returned normally.
     */
    buf = NULL;
    len = saprintf(&buf, 64, "%s", "fits");
    teqz(len, 4);
    tmatch(buf, "fits");
    rFree(buf);
}


/*
    Conversions and modifiers the suite had never asked for. Each of these is a branch in
    innerSprintf or outNum that was at zero coverage: the thousands separator, precision and
    width from arguments, the L and z length modifiers, the octal prefix, and the argument
    guards on the entry points.
 */
static void formatCorners()
{
    char  buf[128];
    ssize rc;

    //  Thousands grouping, both spellings, and no separator for a value under a thousand
    rc = rSnprintf(buf, sizeof(buf), "%,d", 1234567);
    tmatch(buf, "1,234,567");
    teqz(rc, 9);
    rc = rSnprintf(buf, sizeof(buf), "%'d", 999);
    tmatch(buf, "999");
    rc = rSnprintf(buf, sizeof(buf), "%,lld", (int64) -1234567890123LL);
    tmatch(buf, "-1,234,567,890,123");

    //  Precision supplied as an argument, and a precision longer than the string
    rc = rSnprintf(buf, sizeof(buf), "%.*s|", 3, "abcdef");
    tmatch(buf, "abc|");
    rc = rSnprintf(buf, sizeof(buf), "%.10s|", "abc");
    tmatch(buf, "abc|");

    //  "#" on a string means the whole string, whatever the precision says
    rc = rSnprintf(buf, sizeof(buf), "%#.2s", "abcdef");
    tmatch(buf, "abcdef");

    //  Length modifiers: L is 64-bit, z is size_t, l is long
    rc = rSnprintf(buf, sizeof(buf), "%Ld", (int64) 9007199254740993LL);
    tmatch(buf, "9007199254740993");
    rc = rSnprintf(buf, sizeof(buf), "%zu", (size_t) 18446744073709551615ULL);
    tmatch(buf, "18446744073709551615");
    rc = rSnprintf(buf, sizeof(buf), "%lu", (ulong) 4294967295UL);
    tmatch(buf, "4294967295");

    //  Octal with the alternate form takes a single leading zero
    rc = rSnprintf(buf, sizeof(buf), "%#o", 8);
    tmatch(buf, "010");
    rc = rSnprintf(buf, sizeof(buf), "%#x", 255);
    tmatch(buf, "0xff");

    //  A precision so large it saturates rather than wrapping
    rc = rSnprintf(buf, sizeof(buf), "%.99999999999d|", 7);
    ttrue(rc > 0);
    ttrue(slen(buf) < sizeof(buf));

    /*
        1e23 is the classic case where x / 10^floor(log10 x) lands just under 1.0, so the
        mantissa has to be renormalised or the exponent is off by one.
     */
    rc = rSnprintf(buf, sizeof(buf), "%e", 1e23);
    tcontains(buf, "e+23");
    ttrue(buf[0] == '1' || buf[0] == '9');

    //  A type character the parser classifies but has no case for is emitted literally
    rc = rSnprintf(buf, sizeof(buf), "%w");
    ttrue(rc >= 0);

    //  Entry-point guards: no buffer, no room, no format
    rc = rSnprintf(NULL, sizeof(buf), "x");
    teqz(rc, R_ERR_BAD_ARGS);
    rc = rSnprintf(buf, 0, "x");
    teqz(rc, R_ERR_BAD_ARGS);
    buf[0] = 'Z';
    rc = rSnprintf(buf, sizeof(buf), NULL);
    teqz(rc, 0);
    tmatch(buf, "");
}

/*
    rVsaprintf with a maxsize treats it as a hard ceiling: output that would exceed it is not
    truncated but refused, with R_ERR_MEMORY and no buffer, so a caller that set a limit never
    receives a silently shortened string.
 */
static ssize cappedFormat(char **out, size_t max, cchar *fmt, ...)
{
    va_list ap;
    ssize   rc;

    va_start(ap, fmt);
    rc = rVsaprintf(out, max, fmt, ap);
    va_end(ap);
    return rc;
}

static void allocationCap()
{
    char  *out;
    ssize rc;

    out = NULL;
    rc = cappedFormat(&out, 300, "%500d", 1);
    teqz(rc, R_ERR_MEMORY);
    tnull(out);

    //  A cap below the initial allocation is honoured the same way
    out = NULL;
    rc = cappedFormat(&out, 16, "%s", "0123456789abcdefghijklmnop");
    teqz(rc, R_ERR_MEMORY);
    tnull(out);

    //  Output that fits under the cap is delivered in full
    out = NULL;
    rc = cappedFormat(&out, 300, "%200d", 1);
    teqz(rc, 200);
    tnotnull(out);
    teqz((ssize) slen(out), 200);
    rFree(out);
}


/*
    Check that literal and argument-supplied widths and precisions are bounded.
 */
static void resourceCaps()
{
    char  *out;
    Ticks start, elapsed;
    ssize len;

    start = rGetTicks();
    out = sfmt("%*d", INT_MAX, 1);
    elapsed = rGetTicks() - start;
    len = (ssize) slen(out);
    ttrue(len <= ME_R_MAX_PRINTF_WIDTH + 16);
    ttrue(elapsed < 2000);
    rFree(out);

    start = rGetTicks();
    out = sfmt("%2000000000d", 1);
    elapsed = rGetTicks() - start;
    len = (ssize) slen(out);
    ttrue(len <= ME_R_MAX_PRINTF_WIDTH + 16);
    ttrue(elapsed < 2000);
    rFree(out);

    start = rGetTicks();
    out = sfmt("%.*f", 400000000, 1.0);
    elapsed = rGetTicks() - start;
    len = (ssize) slen(out);
    ttrue(len <= ME_R_MAX_PRINTF_PRECISION + 16);
    ttrue(elapsed < 2000);
    rFree(out);

    start = rGetTicks();
    out = sfmt("%.400000000f", 1.0);
    elapsed = rGetTicks() - start;
    len = (ssize) slen(out);
    ttrue(len <= ME_R_MAX_PRINTF_PRECISION + 16);
    ttrue(elapsed < 2000);
    rFree(out);

    start = rGetTicks();
    out = sfmt("%*.*f", INT_MAX, INT_MAX, 1.0);
    elapsed = rGetTicks() - start;
    len = (ssize) slen(out);
    ttrue(len <= ME_R_MAX_PRINTF_WIDTH + ME_R_MAX_PRINTF_PRECISION + 16);
    ttrue(elapsed < 2000);
    rFree(out);
}

/*
    A full buffer must still consume prefix characters and arguments.
 */
static void truncatedSideEffects()
{
    char  buf[16];
    ssize len;

    len = rSnprintf(buf, sizeof(buf), "%*d", 100, -1);
    teqz(len, 100);
    teqi(buf[sizeof(buf) - 1], 0);

    len = rSnprintf(buf, sizeof(buf), "%*x|%#x", 40, 1, 255);
    teqz(len, 45);

    len = rSnprintf(buf, 4, "%s%c%d", "abcdef", 'x', 7);
    teqz(len, 8);
    tmatch(buf, "abc");
}


static void floatRange()
{
    char   *out;
    double value;

    out = sfmt("%f", 1e20);
    tmatch(out, "1.000000e+20");
    rFree(out);

    out = sfmt("%f", -1e20);
    tmatch(out, "-1.000000e+20");
    rFree(out);


    out = sfmt("%f", 9223372036854775808.0);
    tmatch(out, "9.223372e+18");
    rFree(out);

    out = sfmt("%.1f", 9223372036854774784.0);
    tmatch(out, "9223372036854774784.0");
    rFree(out);

    out = sfmt("%.2f", 1e300);
    tmatch(out, "1.00e+300");
    rFree(out);

    out = sfmt("%f", -1.7976931348623157e308);
    tcontains(out, "e+308");
    ttrue(out[0] == '-');
    ttrue(strchr(out, '/') == NULL);
    rFree(out);

    out = sfmt("%g", 1e20);
    tmatch(out, "1e+20");
    rFree(out);

    //  Infinity and NaN
    out = sfmt("%f", (double) INFINITY);
    tmatch(out, "inf");
    rFree(out);

    out = sfmt("%f", -(double) INFINITY);
    tmatch(out, "-inf");
    rFree(out);

    out = sfmt("%f", (double) NAN);
    tmatch(out, "nan");
    rFree(out);

    out = sfmt("%e", (double) NAN);
    tmatch(out, "nan");
    rFree(out);

    out = sfmt("%g", (double) INFINITY);
    tmatch(out, "inf");
    rFree(out);

    out = sfmt("%E", -(double) INFINITY);
    tmatch(out, "-INF");
    rFree(out);

    out = sfmt("%G", (double) NAN);
    tmatch(out, "NAN");
    rFree(out);


    out = sfmt("%+f", (double) INFINITY);
    tmatch(out, "+inf");
    rFree(out);

    out = sfmt("% f", (double) INFINITY);
    tmatch(out, " inf");
    rFree(out);

    out = sfmt("%8f|", (double) INFINITY);
    tmatch(out, "     inf|");
    rFree(out);

    out = sfmt("%-8f|", -(double) INFINITY);
    tmatch(out, "-inf    |");
    rFree(out);

    out = sfmt("%08.2f", (double) NAN);
    tmatch(out, "     nan");
    rFree(out);

    out = sfmt("%.1f", (double) INFINITY);
    tmatch(out, "inf");
    rFree(out);

    //  A non-finite value consumes its argument, so those that follow are not displaced
    out = sfmt("%f|%d|%f", (double) NAN, 7, (double) INFINITY);
    tmatch(out, "nan|7|inf");
    rFree(out);

    //  The smallest denormal has no representable power of ten to divide by
    value = 4.9406564584124654e-324;
    out = sfmt("%e", value);
    ttrue(strncmp(out, "4.94", 4) == 0);
    tcontains(out, "e-324");
    rFree(out);
}



static void floatDigits()
{
    static cchar *specs[] = { "%f", "%e", "%g", "%.20f", "%.15e", "%G" };
    static double mantissas[] = { 1.0, 1.5, 9.999999999999998, 9.9999999999999999, 0.99999999999999989 };
    char          *out;
    double        value;
    int           e, i, j;

    for (i = 0; i < (int) (sizeof(specs) / sizeof(specs[0])); i++) {
        for (j = 0; j < (int) (sizeof(mantissas) / sizeof(mantissas[0])); j++) {
            for (e = -323; e <= 308; e += 13) {
                value = mantissas[j] * pow(10.0, e < -300 ? -300 : e);
                if (e < -300) {
                    value *= pow(10.0, e + 300);
                }
                out = sfmt(specs[i], value);
                ttrue(strspn(out, "0123456789.-+eE") == slen(out));
                rFree(out);

                out = sfmt(specs[i], -value);
                ttrue(strspn(out, "0123456789.-+eE") == slen(out));
                rFree(out);
            }
        }
    }
}


int main(void)
{
    rInit(0, 0);
    floatRange();
    floatDigits();
    printfOutput();
    printfGrowth();
    basicSprintf();
    itostring();
    typeOptions();
    floatValues();
    modifierOptions();
    widthOptions();
    precisionOptions();
    bitOptions();
    sprintf64();
    overflow();
    extra();
    regress();
    formatCorners();
    allocationCap();
    resourceCaps();
    truncatedSideEffects();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a
    commercial license from Embedthis Pty Ltd. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.
 */
