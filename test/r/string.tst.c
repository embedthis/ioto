/**
    string.tst.c - Unit tests for the String class

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "r.h"

/************************************ Code ************************************/

static void sitosTest()
{
    char  buf[64], *cp;
    int   radix;
    int64 value;

    tmatch(sitosbuf(buf, sizeof(buf), 0, 10), "0");
    tmatch(sitosbuf(buf, sizeof(buf), -1, 10), "-1");
    tmatch(sitosbuf(buf, sizeof(buf), 1234, 10), "1234");
    tmatch(sitosbuf(buf, sizeof(buf), 8, 2), "1000");

    cp = sitosx(42, 10);
    tmatch(cp, "42");
    rFree(cp);

    cp = sitos(42);
    tmatch(cp, "42");
    rFree(cp);

    for (radix = 2; radix <= 36; radix++) {
        tnotnull(sitosbuf(buf, sizeof(buf), 12345, radix));
        value = stoix(buf, NULL, radix);
        teqi((int) value, 12345);

        tnotnull(sitosbuf(buf, sizeof(buf), -12345, radix));
        value = stoix(buf, NULL, radix);
        teqi((int) value, -12345);
    }
    tmatch(sitosbuf(buf, sizeof(buf), 35, 36), "Z");

    tnull(sitosbuf(buf, sizeof(buf), 20, 37));
    tnull(sitosbuf(buf, sizeof(buf), 20, 1));
}

static void scaseTest()
{
    char buf[64], *cp;

    cp = scamel(NULL);
    tmatch(cp, "");
    rFree(cp);

    cp = scamel("HELLO");
    tmatch(cp, "hELLO");
    rFree(cp);

    teqi(scaselesscmp("hello", "Hello"), 0);
    ttrue(scaselessmatch("hello", "Hello"));

    /*
        sncaselesscmp compares at most n characters and orders by the first difference.
     */
    teqi(sncaselesscmp("HELLO", "hello", 5), 0);
    teqi(sncaselesscmp("HELLO", "hellp", 5), -1);
    teqi(sncaselesscmp("hellp", "HELLO", 5), 1);
    teqi(sncaselesscmp("HELLO", "hellp", 4), 0);
    teqi(sncaselesscmp("abc", "abcdef", 3), 0);
    teqi(sncaselesscmp("abc", "abcdef", 6), -1);
    teqi(sncaselesscmp("abc", "abc", 0), 0);

    scopy(buf, sizeof(buf), "Hello");
    tmatch(slower(buf), "hello");
    tmatch(supper(buf), "HELLO");

    cp = stitle("hello");
    tmatch(cp, "Hello");
    rFree(cp);
}

static void smatchTest()
{
    char *cp;

    ttrue(schr("Hello", 'o'));
    tfalse(schr("Hello", 'z'));

    ttrue(scontains("Hello", "ell"));
    tfalse(scontains("Hello", "world"));
    ttrue(sncontains("Hello", "ell", 6));
    ttrue(sncontains("Hello", "ell", 0));
    tfalse(sncontains("Hello", "ell", 2));
    tfalse(sncontains("Hello", 0, 0));
    tfalse(sncontains(0, 0, 0));
    ttrue(sncaselesscontains("Hello", "hello", 5));
    teqi(sncaselesscmp("Hello", "hello", 5), 0);

    /*
        Exercise bytes 0x80-0xFF, which are invalid signed-char inputs to tolower.
     */
    {
        char hi[3];
        int  i;

        for (i = 0x80; i <= 0xFF; i++) {
            hi[0] = (char) i;
            hi[1] = (char) i;
            hi[2] = '\0';
            ttrue(sncaselesscontains(hi, hi, 2) != 0);
            teqi(sncaselesscmp(hi, hi, 2), 0);
        }
    }

    teqi(scmp("Hello", "World"), -1);
    teqi(scmp("World", "Hello"), 1);
    teqi(scmp("Hello", "Hello"), 0);

    teqi(sncmp("Hello", "World", 2), -1);
    teqi(sncmp("ABC", "Abc", 1), 0);

    tmatch(sends("Hello", "lo"), "lo");
    tnull(sends("Hello", "World"));

    cp = spbrk("Hello World", " ");
    tmatch(cp, " World");

    cp = schr("Hello World ", ' ');
    tmatch(cp, " World ");
    cp = srchr("Hello World ", ' ');
    tmatch(cp, " ");
}


static void scmpUnsignedTest()
{
    cchar *s[] = { "", "t", "t\xF0", "t\xF0x", "t'", "t\x7f", "tz", "t\xC3\xA9", "t\xF0\xF0", "\xFF" };
    int   i, j, n, ij, ji, sc;

    n = (int) (sizeof(s) / sizeof(s[0]));
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            ij = scmp(s[i], s[j]);
            ji = scmp(s[j], s[i]);
            sc = strcmp(s[i], s[j]);
            sc = (sc > 0) ? 1 : (sc < 0) ? -1 : 0;
            teqi(ij, -ji);
            teqi(ij, sc);
        }
    }
    teqi(scmp("t\xF0", "t"), 1);
    teqi(scmp("t", "t\xF0"), -1);
    teqi(sncmp("t\xF0", "t'", 2), 1);
    teqi(sncmp("t'", "t\xF0", 2), -1);
}

static void scopyTest()
{
    char  buf[64], *cp;
    ssize len;

    len = scopy(buf, sizeof(buf), "Hello");
    teqz(len, 5);
    tmatch(buf, "Hello");

    len = sncopy(buf, sizeof(buf), "Hello", 2);
    teqz(len, 2);
    tmatch(buf, "He");


    cp = sclone("Hello");
    tmatch(cp, "Hello");
    rFree(cp);
    cp = sclone(0);
    tmatch(cp, "");
    rFree(cp);

    cp = snclone("Hello", 2);
    tmatch(cp, "He");
    rFree(cp);
}

/*
    Allocate exactly n bytes so ASan detects scans beyond len.
 */
static void sncloneUnterminatedTest()
{
    char  *block, *cp;
    ssize n;

    n = 5;
    block = malloc((size_t) n);
    memcpy(block, "Hello", (size_t) n);

    cp = snclone(block, n);
    tmatch(cp, "Hello");
    teqz((ssize) slen(cp), n);
    rFree(cp);
    free(block);
}

static void sfmtTest()
{
    char buf[256], *cp;

#if __GNUC__
    #pragma GCC diagnostic ignored "-Wformat-zero-length"
#endif
    cp = sfmt("");
    tmatch(cp, "");
    rFree(cp);
    cp = sfmt("Hello %s", "World");
    tmatch(cp, "Hello World");
    rFree(cp);

    tmatch(sfmtbuf(buf, sizeof(buf), "Hello %s", "World"), "Hello World");

    cp = NULL;
    cp = sfmt("Hello %s", cp);
    tmatch(cp, "Hello null");
    rFree(cp);
}

static void shashTest()
{
    tnotnull((void*) (ssize) shash("Hello World", 11));
    tnotnull((void*) (ssize) shashlower("Hello World", 11));
}

static void sopsTest()
{
    char  *argv[2];
    char  buf[128], *cp;
    ssize len;

    cp = sjoin("Hello", "World", 0);
    tmatch(cp, "HelloWorld");
    rFree(cp);

    cp = sjoinfmt("Hello", " %s", "World");
    tmatch(cp, "Hello World");
    rFree(cp);

    len = sjoinbuf(buf, sizeof(buf), "Hello", "World");
    teqz(len, 10);
    tmatch(buf, "HelloWorld");

    argv[0] = "Hello";
    argv[1] = "World";
    cp = sjoinArgs(2, (cchar**) argv, " ");
    tmatch(cp, "Hello World");
    rFree(cp);
}

static void snumTest()
{
    ttrue(snumber("0"));
    ttrue(snumber("1234"));
    tfalse(snumber("abc1234"));
    tfalse(snumber("1234 "));
    tfalse(snumber("1234.345"));

    ttrue(shnumber("0x4"));
    ttrue(shnumber("0"));

    ttrue(sfnumber("1234.345"));
    ttrue(sfnumber("0"));
    ttrue(sfnumber("-1"));
}

static void stemplateTest()
{
    RHash *keys;
    char  *cp;

    keys = rAllocHash(0, 0);
    rAddName(keys, "greeting", "Hello", 0);
    rAddName(keys, "scope", "World", 0);

    cp = stemplate("${greeting} ${scope}", keys);
    tmatch(cp, "Hello World");
    rFree(cp);

    cp = stemplate("${}", keys);
    tmatch(cp, "${}");
    rFree(cp);

    cp = stemplate("${", keys);
    tmatch(cp, "${");
    rFree(cp);

    cp = stemplate("abc${", keys);
    tmatch(cp, "abc${");
    rFree(cp);

    cp = stemplate("${x", keys);
    tmatch(cp, "${x");
    rFree(cp);

    cp = stemplate("${greeting} abc${", keys);
    tmatch(cp, "Hello abc${");
    rFree(cp);

    rFreeHash(keys);
}


static void stemplateUnterminatedTest()
{
    RHash *keys;
    char  *cp;
    char  plain[] = "${greeting\0TAIL";
    char  prefixed[] = "abc${greeting\0TAIL";
    char  bare[] = "$greeting\0TAIL";
    char  braced[] = "${greeting}\0TAIL";
    char  empty[] = "${\0TAIL";

    keys = rAllocHash(0, 0);
    rAddName(keys, "greeting", "hello", 0);

    cp = stemplate(plain, keys);
    tmatch(cp, "hello");
    rFree(cp);

    cp = stemplate(prefixed, keys);
    tmatch(cp, "abchello");
    rFree(cp);

    cp = stemplate(bare, keys);
    tmatch(cp, "hello");
    rFree(cp);

    cp = stemplate(braced, keys);
    tmatch(cp, "hello");
    rFree(cp);

    cp = stemplate(empty, keys);
    tmatch(cp, "${");
    rFree(cp);

    rFreeHash(keys);
}

static void testUncoveredFunctions()
{
    char   *cp, *cp2, *last;
    char   buf[128];
    size_t len;
    double d;
    int64  i;

    /*
        Test scloneNull
     */
    cp = scloneNull("hello");
    tmatch(cp, "hello");
    rFree(cp);

    cp = scloneNull(NULL);
    tnull(cp);

    /*
        Test sspace
     */
    ttrue(sspace("   \t\n"));
    ttrue(sspace(""));
    tfalse(sspace("hello"));
    tfalse(sspace("  a  "));

    /*
        Test svalue (parse values with K, M, G suffixes)
     */
    teqz(svalue("1024"), 1024);
    teqz(svalue("1K"), 1024);
    teqz(svalue("1M"), 1024 * 1024);
    teqz(svalue("1G"), 1024 * 1024 * 1024);
    teqz(svalue("invalid"), 0);

    /*
        Test sreplace
     */
    cp = sreplace("hello world", "world", "universe");
    tmatch(cp, "hello universe");
    rFree(cp);

    cp = sreplace("hello", "xyz", "abc");
    tmatch(cp, "hello");
    rFree(cp);

    /*
        Test ssplit - note: this modifies the original string
     */
    scopy(buf, sizeof(buf), "one,two,three");
    cp = ssplit(buf, ",", &last);
    tmatch(cp, "one");
    tmatch(last, "two,three");

    /*
        Test sspn
     */
    len = sspn("hello123", "helo");
    teqz(len, 5);

    /*
        Test sstarts
     */
    ttrue(sstarts("hello world", "hello"));
    tfalse(sstarts("hello world", "world"));
    tfalse(sstarts("hi", "hello"));

    /*
        Test string to number conversions
     */
    d = stod("123.45");
    ttrue(d > 123.4 && d < 123.5);

    d = stof("98.7");
    ttrue(d > 98.6 && d < 98.8);

    i = stoi("12345");
    teqz(i, 12345);

    i = stoix("FF", NULL, 16);
    teqz(i, 255);

    i = stoix("invalid", NULL, 10);
    teqz(i, 0);

    /*
        Test stok (tokenizer)
     */
    scopy(buf, sizeof(buf), "apple banana cherry");
    cp = stok(buf, " ", &last);
    tmatch(cp, "apple");
    cp = stok(NULL, " ", &last);
    tmatch(cp, "banana");
    cp = stok(NULL, " ", &last);
    tmatch(cp, "cherry");

    /*
        Test sptok (pattern tokenizer)
     */
    scopy(buf, sizeof(buf), "word1::word2::word3");
    cp = sptok(buf, "::", &cp2);
    tmatch(cp, "word1");
    tmatch(cp2, "word2::word3");

    /*
        Test ssub
     */
    cp = ssub("hello world", 0, 5);
    tmatch(cp, "hello");
    rFree(cp);

    cp = ssub("hello", 1, 3);
    tmatch(cp, "ell");
    rFree(cp);

    /*
        Test strim
     */
    scopy(buf, sizeof(buf), "  hello  ");
    cp = strim(buf, " ", R_TRIM_BOTH);
    tmatch(cp, "hello");

    scopy(buf, sizeof(buf), "xxxhelloxxx");
    cp = strim(buf, "x", R_TRIM_BOTH);
    tmatch(cp, "hello");

    /*
        Test szero
     */
    scopy(buf, sizeof(buf), "sensitive");
    szero(buf);
    teqi(buf[0], 0);

    /*
        Note: stolist function is not currently available (commented out in source)
     */
}

static void testErrorConditions()
{
    char buf[4];
    char *cp;

    /*
        Test sitosbuf edge cases
     */
    cp = sitosbuf(buf, 2, 1234, 10);
    tnull(cp);

    cp = sitosbuf(buf, sizeof(buf), 0, 0);
    tmatch(cp, "0");

    cp = sitosbuf(buf, sizeof(buf), INT64_MIN, 10);
    tnull(cp);

    cp = sitosbuf(buf, sizeof(buf), 20, 37);
    tnull(cp);

    cp = sitosbuf(buf, sizeof(buf), 20, 1);
    tnull(cp);

    /*
        Test various NULL safety
     */
    teqz(slen(NULL), 0);
    teqi(scmp(NULL, NULL), 0);
    teqi(scmp("test", NULL), 1);
    teqi(scmp(NULL, "test"), -1);
}

static void testStringManipulation()
{
    char *cp, *result;

    /*
        Test srejoin
     */
    cp = sclone("hello");
    result = srejoin(cp, " world", NULL);
    tmatch(result, "hello world");
    rFree(result);
}

/*
    sparseUint is the strict wire-format parser. Unlike stoix it must report failure rather
    than return a clamped value, so every rejection below is the point of the function.
 */
static void sparseUintTest()
{
    char  *end;
    int64 value;
    int   rc;

    /*
        Results are bound to a local before every assertion. The TestMe comparison macros
        expand their arguments twice, so a call placed directly in one runs twice.
     */
    rc = sparseUint("0", 10, 255, NULL, &value);
    teqi(rc, 0);
    teqz(value, 0);
    rc = sparseUint("42", 10, 255, NULL, &value);
    teqi(rc, 0);
    teqz(value, 42);
    rc = sparseUint("255", 10, 255, NULL, &value);
    teqi(rc, 0);
    teqz(value, 255);

    rc = sparseUint("ff", 16, 255, NULL, &value);
    teqi(rc, 0);
    teqz(value, 255);
    rc = sparseUint("FF", 16, 255, NULL, &value);
    teqi(rc, 0);
    teqz(value, 255);
    rc = sparseUint("dead", 16, 0xFFFF, NULL, &value);
    teqi(rc, 0);
    teqz(value, 0xDEAD);

    /*
        At the limit, then one past it. Saturation would be a framing divergence from a peer
        that parses the same bytes correctly, so the one past must be an error.
     */
    rc = sparseUint("65535", 10, 65535, NULL, &value);
    teqi(rc, 0);
    teqz(value, 65535);
    rc = sparseUint("65536", 10, 65535, NULL, &value);
    teqi(rc, R_ERR_WONT_FIT);
    rc = sparseUint("99999999999999999999999", 10, MAXINT64, NULL, &value);
    teqi(rc, R_ERR_WONT_FIT);

    /*
        A single digit over the ceiling. The pre-multiply overflow guard divides, and the
        division truncates to zero for a small max, so the bound has to be tested on the result
        as well or the first digit escapes the check entirely.
     */
    rc = sparseUint("9", 10, 5, NULL, &value);
    teqi(rc, R_ERR_WONT_FIT);
    rc = sparseUint("5", 10, 5, NULL, &value);
    teqi(rc, 0);
    teqz(value, 5);
    rc = sparseUint("1", 10, 0, NULL, &value);
    teqi(rc, R_ERR_WONT_FIT);
    rc = sparseUint("f", 16, 10, NULL, &value);
    teqi(rc, R_ERR_WONT_FIT);
    rc = sparseUint("a", 16, 10, NULL, &value);
    teqi(rc, 0);
    teqz(value, 10);

    /*
        A rejected parse must leave the output at zero rather than partially assigned.
     */
    value = 12345;
    rc = sparseUint("65536", 10, 65535, NULL, &value);
    teqi(rc, R_ERR_WONT_FIT);
    teqz(value, 0);

    /*
        No sign, no leading whitespace, no empty string. strtol accepts all three.
     */
    rc = sparseUint("-1", 10, 255, NULL, &value);
    teqi(rc, R_ERR_BAD_ARGS);
    rc = sparseUint("+1", 10, 255, NULL, &value);
    teqi(rc, R_ERR_BAD_ARGS);
    rc = sparseUint(" 1", 10, 255, NULL, &value);
    teqi(rc, R_ERR_BAD_ARGS);
    rc = sparseUint("", 10, 255, NULL, &value);
    teqi(rc, R_ERR_BAD_ARGS);
    rc = sparseUint("x", 10, 255, NULL, &value);
    teqi(rc, R_ERR_BAD_ARGS);

    /*
        Hex digits are not decimal digits, and a radix other than 10 or 16 is rejected.
     */
    rc = sparseUint("ff", 10, 255, NULL, &value);
    teqi(rc, R_ERR_BAD_ARGS);
    rc = sparseUint("10", 8, 255, NULL, &value);
    teqi(rc, R_ERR_BAD_ARGS);
    rc = sparseUint("10", 2, 255, NULL, &value);
    teqi(rc, R_ERR_BAD_ARGS);

    rc = sparseUint(NULL, 10, 255, NULL, &value);
    teqi(rc, R_ERR_BAD_ARGS);
    rc = sparseUint("1", 10, 255, NULL, NULL);
    teqi(rc, R_ERR_BAD_ARGS);

    /*
        The caller decides whether a remainder is acceptable, so end must point at the first
        byte not consumed - including for a parse that consumed nothing.
     */
    rc = sparseUint("12abc", 10, 255, &end, &value);
    teqi(rc, 0);
    teqz(value, 12);
    tmatch(end, "abc");

    rc = sparseUint("7", 10, 255, &end, &value);
    teqi(rc, 0);
    tmatch(end, "");

    end = NULL;
    rc = sparseUint("-5", 10, 255, &end, &value);
    teqi(rc, R_ERR_BAD_ARGS);
    tmatch(end, "-5");
}

/*
    smatchsec is the constant-time comparison used where a timing difference would leak. Only
    the result is asserted here; the timing property is a property of the implementation.
 */
static void smatchsecTest()
{
    ttrue(smatchsec("secret", "secret"));
    ttrue(smatchsec("", ""));
    tfalse(smatchsec("secret", "secre"));
    tfalse(smatchsec("secre", "secret"));
    tfalse(smatchsec("secret", "SECRET"));
    tfalse(smatchsec("secret", ""));
    tfalse(smatchsec("", "secret"));

    /*
        Same length, differing in the first and in the last byte. A comparison that returned
        early would still pass these, but a comparison that ignored a position would not.
     */
    tfalse(smatchsec("aaaaaa", "baaaaa"));
    tfalse(smatchsec("aaaaaa", "aaaaab"));

    ttrue(smatchsec(NULL, NULL));
    tfalse(smatchsec("secret", NULL));
    tfalse(smatchsec(NULL, "secret"));
}

/*
    sncat is the bounded concatenation that replaces strcat. destMax counts the terminator, so
    a source that exactly fills the buffer must be refused, not truncated.
 */
static void sncatTest()
{
    char  buf[16];
    ssize len;

    scopy(buf, sizeof(buf), "Hello");
    len = sncat(buf, sizeof(buf), " World");
    teqz(len, 11);
    tmatch(buf, "Hello World");

    scopy(buf, sizeof(buf), "");
    len = sncat(buf, sizeof(buf), "abc");
    teqz(len, 3);
    tmatch(buf, "abc");

    scopy(buf, sizeof(buf), "abc");
    len = sncat(buf, sizeof(buf), "");
    teqz(len, 3);
    tmatch(buf, "abc");

    /*
        At capacity: 15 characters plus the terminator fit in 16 and the result is terminated.
        One more does not fit, and the destination is left untouched rather than truncated.
     */
    scopy(buf, sizeof(buf), "0123456789");
    len = sncat(buf, sizeof(buf), "abcde");
    teqz(len, 15);
    tmatch(buf, "0123456789abcde");
    teqi(buf[15], 0);

    scopy(buf, sizeof(buf), "0123456789");
    len = sncat(buf, sizeof(buf), "abcdef");
    teqz(len, R_ERR_WONT_FIT);
    tmatch(buf, "0123456789");

    len = sncat(NULL, sizeof(buf), "abc");
    teqz(len, R_ERR_BAD_ARGS);
    len = sncat(buf, sizeof(buf), NULL);
    teqz(len, R_ERR_BAD_ARGS);
    len = sncat(buf, 0, "abc");
    teqz(len, R_ERR_BAD_ARGS);
    len = sncat(buf, sizeof(buf), buf);
    teqz(len, R_ERR_BAD_ARGS);
}

/*
    Varargs entry points. Each has a "..." sibling that the suite already exercises; these are
    the va_list forms an embedding application calls from its own wrappers.
 */
static char *fmtvWrapper(cchar *fmt, ...)
{
    va_list args;
    char    *result;

    va_start(args, fmt);
    result = sfmtv(fmt, args);
    va_end(args);
    return result;
}

static char *fmtbufvWrapper(char *buf, size_t size, cchar *fmt, ...)
{
    va_list args;
    char    *result;

    va_start(args, fmt);
    result = sfmtbufv(buf, size, fmt, args);
    va_end(args);
    return result;
}

static char *joinvWrapper(cchar *str, ...)
{
    va_list args;
    char    *result;

    va_start(args, str);
    result = sjoinv(str, args);
    va_end(args);
    return result;
}

static char *rejoinvWrapper(char *buf, ...)
{
    va_list args;
    char    *result;

    va_start(args, buf);
    result = srejoinv(buf, args);
    va_end(args);
    return result;
}

static ssize vsnprintfWrapper(char *buf, size_t size, cchar *fmt, ...)
{
    va_list args;
    ssize   count;

    va_start(args, fmt);
    count = rVsnprintf(buf, size, fmt, args);
    va_end(args);
    return count;
}

static ssize vsaprintfWrapper(char **buf, size_t size, cchar *fmt, ...)
{
    va_list args;
    ssize   count;

    va_start(args, fmt);
    count = rVsaprintf(buf, size, fmt, args);
    va_end(args);
    return count;
}

static void svarargsTest()
{
    char  buf[64], *cp;
    ssize count;

    cp = fmtvWrapper("Hello %s %d", "World", 42);
    tmatch(cp, "Hello World 42");
    rFree(cp);

    fmtbufvWrapper(buf, sizeof(buf), "%s-%d", "id", 7);
    tmatch(buf, "id-7");

    /*
        Formatting into a buffer that cannot hold the result truncates and terminates.
     */
    fmtbufvWrapper(buf, 4, "%s", "abcdefgh");
    tmatch(buf, "abc");

    cp = joinvWrapper("a", "b", "c", NULL);
    tmatch(cp, "abc");
    rFree(cp);

    cp = rejoinvWrapper(sclone("a"), "b", "c", NULL);
    tmatch(cp, "abc");
    rFree(cp);

    count = vsnprintfWrapper(buf, sizeof(buf), "%d-%s", 5, "five");
    teqz(count, 6);
    tmatch(buf, "5-five");

    cp = NULL;
    count = vsaprintfWrapper(&cp, 64, "%s=%d", "n", 9);
    teqz(count, 3);
    tmatch(cp, "n=9");
    rFree(cp);
}

/*
    scloneDefined distinguishes "absent" from "present but empty" - both return NULL - where
    sclone returns an empty string and scloneNull returns NULL only for absent.
 */
static void scloneDefinedTest()
{
    char *cp;

    cp = scloneDefined("hello");
    tmatch(cp, "hello");
    rFree(cp);

    cp = scloneDefined("");
    tnull(cp);
    cp = scloneDefined(NULL);
    tnull(cp);

    /*
        The contrast that gives the function its name.
     */
    cp = scloneNull("");
    tmatch(cp, "");
    rFree(cp);
}

/*
    svaluei is svalue clamped to int. The clamp is the behaviour worth pinning.
 */
static void svalueiTest()
{
    teqi(svaluei("1024"), 1024);
    teqi(svaluei("1K"), 1024);
    teqi(svaluei("0"), 0);
    teqi(svaluei("unlimited"), INT_MAX);
    teqi(svaluei("2G"), INT_MAX);
    teqi(svaluei("invalid"), 0);

    /*
        The suffix table in svalue that svalue's own test does not reach.
     */
    teqz(svalue("2mins"), 120);
    teqz(svalue("1hour"), 3600);
    teqz(svalue("1day"), 86400);
    teqz(svalue("1week"), 604800);
    teqz(svalue("1sec"), 1);
    teqz(svalue("infinite"), MAXINT64);

    /*
        Longer than the internal 80-byte scratch buffer: rejected, not overflowed.
     */
    teqz(svalue("11111111111111111111111111111111111111111111111111"
                "111111111111111111111111111111111"), 0);
}

/*
    The branches the string suite had walked past: the null and length asymmetries in the
    bounded compares, a full destination in sncat, the unit words svalue accepts, and the
    defaults stok, sptok and strim apply when handed nothing.
 */
static void stringCorners()
{
    char  buf[8], text[32];
    cchar *argv[3];
    char  *cp, *last;

    //  Bounded compares: one side null, and one side shorter than the bound
    teqi(sncmp(NULL, "a", 5), -1);
    teqi(sncmp("a", NULL, 5), 1);
    teqi(sncmp("ab", "abc", 10), -1);
    teqi(sncmp("abc", "ab", 10), 1);
    teqi(sncmp("abc", "abc", 10), 0);
    teqi(sncaselesscmp(NULL, "a", 5), -1);
    teqi(sncaselesscmp("a", NULL, 5), 1);
    teqi(sncaselesscmp("AB", "abc", 10), -1);
    teqi(sncaselesscmp("ABC", "ab", 10), 1);
    teqi(sncaselesscmp("ABC", "abc", 10), 0);

    //  sncat when the destination already fills its bound
    scopy(buf, sizeof(buf), "ab");
    teqz(sncat(buf, 2, "x"), R_ERR_WONT_FIT);
    tmatch(buf, "ab");

    //  sjoinbuf when the second string is the one that does not fit
    ttrue(sjoinbuf(buf, 4, "ab", "cdef") < 0);

    //  sjoinArgs with no separator
    argv[0] = "a"; argv[1] = "b"; argv[2] = "c";
    cp = sjoinArgs(3, argv, NULL);
    tmatch(cp, "abc");
    rFree(cp);

    //  spbrk with nothing from the set present
    tnull(spbrk("abc", "xyz"));
    tnotnull(spbrk("abc", "c"));

    //  svalue: the two open-ended words and the calendar units
    teqll(svalue("never"), 7260757200000LL);
    teqll(svalue("forever"), 7260757200000LL);
    teqll(svalue("unlimited"), MAXINT64);
    teqll(svalue("2 months"), 2LL * 60 * 60 * 24 * 30);
    teqll(svalue("1 year"), 60LL * 60 * 24 * 365);

    //  svaluei clamps in both directions
    teqi(svaluei("-99999999999"), INT_MIN);
    teqi(svaluei("99999999999"), INT_MAX);

    //  ssplit: no delimiter set returns the string whole; no match leaves last empty
    scopy(text, sizeof(text), "a,b");
    cp = ssplit(text, NULL, &last);
    tmatch(cp, "a,b");
    scopy(text, sizeof(text), "abc");
    cp = ssplit(text, ",", &last);
    tmatch(cp, "abc");
    tmatch(last, "");

    //  stok and sptok with nothing to work from
    tnull(stok(NULL, ",", NULL));
    tnull(sptok(NULL, ",", NULL));

    //  strim with where == 0 trims both ends
    scopy(text, sizeof(text), "  x  ");
    cp = strim(text, " ", 0);
    tmatch(cp, "x");

    //  Null guards on the formatters and hashers
    tnull(sfmtbuf(NULL, 8, "x"));
    tnull(sfmtbuf(buf, 8, NULL));
    tnull(sfmtbuf(buf, 0, "x"));
    teqi((int) shash(NULL, 0), 0);
    teqi((int) shashlower(NULL, 0), 0);
}


int main(void)
{
    rInit(0, 0);
    sitosTest();
    scaseTest();
    smatchTest();
    scmpUnsignedTest();
    scopyTest();
    sncloneUnterminatedTest();
    sfmtTest();
    shashTest();
    sopsTest();
    snumTest();
    stemplateTest();
    stemplateUnterminatedTest();

    testUncoveredFunctions();
    testErrorConditions();
    testStringManipulation();

    sparseUintTest();
    smatchsecTest();
    sncatTest();
    svarargsTest();
    scloneDefinedTest();
    svalueiTest();
    stringCorners();

    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
