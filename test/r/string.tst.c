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
    char buf[64], *cp;

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
    rFreeHash(keys);
}

static void testUncoveredFunctions()
{
    char   *cp, *cp2, *last;
    char   buf[128];
    ssize  len;
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

int main(void)
{
    rInit(0, 0);
    sitosTest();
    scaseTest();
    smatchTest();
    scopyTest();
    sfmtTest();
    shashTest();
    sopsTest();
    snumTest();
    stemplateTest();

    testUncoveredFunctions();
    testErrorConditions();
    testStringManipulation();

    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
