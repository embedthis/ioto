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

int main(void)
{
    rInit(0, 0);
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
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.
 */
