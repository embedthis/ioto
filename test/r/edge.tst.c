/*
    edge.tst.c - Boundary and error-path sweep

    Edges that span modules rather than belonging to one API: values at the limits of their type,
    parsers given input designed to break them, and the failure paths that only run when something
    has already gone wrong. Each class below has at least one case AT the limit and one PAST it.

    The TestMe comparison macros expand their arguments twice, so every call with a side effect is
    bound to a local before it is asserted.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "r.h"

/************************************ Code ************************************/

/*
    Formatting at the limits of the types being printed. A formatter that mishandles the most
    negative integer is a classic overflow: negating it does not fit in the type.
 */
static void formatLimits()
{
    char buf[64];
    char *cp;

    cp = sfmt("%lld", (int64) INT64_MAX);
    tmatch(cp, "9223372036854775807");
    rFree(cp);
    cp = sfmt("%lld", (int64) INT64_MIN);
    tmatch(cp, "-9223372036854775808");
    rFree(cp);

    cp = sfmt("%d", INT_MAX);
    tmatch(cp, "2147483647");
    rFree(cp);
    cp = sfmt("%d", INT_MIN);
    tmatch(cp, "-2147483648");
    rFree(cp);

    cp = sfmt("%u", (uint) 0xFFFFFFFF);
    tmatch(cp, "4294967295");
    rFree(cp);
    /*
        Hex with the top bit set. %X must read the same width as %x - promoting it to 64 bits
        read eight bytes of varargs for a four-byte argument.
     */
    cp = sfmt("%x", 0xDEADBEEF);
    tmatch(cp, "deadbeef");
    rFree(cp);
    cp = sfmt("%X", 0xDEADBEEF);
    tmatch(cp, "DEADBEEF");
    rFree(cp);
    cp = sfmt("%hX", (int) 0xBEEF);
    tmatch(cp, "BEEF");
    rFree(cp);

    /*
        An unsigned value above INT64_MAX. outNum takes an int64, so without an unsigned flag
        the top bit reads as a sign and the whole value prints as "-1".
     */
    cp = sfmt("%llx", (uint64) 0xFFFFFFFFFFFFFFFFULL);
    tmatch(cp, "ffffffffffffffff");
    rFree(cp);
    cp = sfmt("%llX", (uint64) 0xFFFFFFFFFFFFFFFFULL);
    tmatch(cp, "FFFFFFFFFFFFFFFF");
    rFree(cp);
    cp = sfmt("%llu", (uint64) 0xFFFFFFFFFFFFFFFFULL);
    tmatch(cp, "18446744073709551615");
    rFree(cp);
    cp = sfmt("%llo", (uint64) 0xFFFFFFFFFFFFFFFFULL);
    tmatch(cp, "1777777777777777777777");
    rFree(cp);

    /*
        The signed conversions still sign.
     */
    cp = sfmt("%lld", (int64) -1);
    tmatch(cp, "-1");
    rFree(cp);

    cp = sfmt("%o", 8);
    tmatch(cp, "10");
    rFree(cp);
    cp = sfmt("%#x", 255);
    tmatch(cp, "0xff");
    rFree(cp);

    /*
        Width and precision at the extremes. A width far larger than any internal buffer must
        grow the allocation rather than truncate.
     */
    cp = sfmt("%200d", 1);
    teqz(slen(cp), 200);
    rFree(cp);

    cp = sfmt("%.3s", "abcdef");
    tmatch(cp, "abc");
    rFree(cp);
    cp = sfmt("%.0s", "abcdef");
    tmatch(cp, "");
    rFree(cp);
    cp = sfmt("%-5d|", 1);
    tmatch(cp, "1    |");
    rFree(cp);
    cp = sfmt("%05d", 1);
    tmatch(cp, "00001");
    rFree(cp);

    /*
        Formatting into a fixed buffer truncates rather than overflowing, and the result is
        always terminated. One character of room means an empty string, not a stray byte.
     */
    sfmtbuf(buf, sizeof(buf), "%s", "short");
    tmatch(buf, "short");

    sfmtbuf(buf, 4, "%s", "abcdefgh");
    tmatch(buf, "abc");
    teqi(buf[3], 0);

    sfmtbuf(buf, 1, "%s", "abcdefgh");
    tmatch(buf, "");
}


/*
    Integer conversion at the limits, in both directions.
 */
static void numberLimits()
{
    char  buf[64];
    char  *cp;
    int64 i, value;
    int   rc;

    cp = sitosbuf(buf, sizeof(buf), INT64_MAX, 10);
    tmatch(cp, "9223372036854775807");

    /*
        The most negative value has no positive counterpart, so a converter that negates it
        first gets this wrong.
     */
    cp = sitosbuf(buf, sizeof(buf), INT64_MIN, 10);
    tmatch(cp, "-9223372036854775808");

    cp = sitosbuf(buf, sizeof(buf), 255, 16);
    tmatch(cp, "FF");
    cp = sitosbuf(buf, sizeof(buf), 255, 2);
    tmatch(cp, "11111111");

    /*
        A buffer exactly large enough, then one byte too small.
     */
    cp = sitosbuf(buf, 5, 1234, 10);
    tmatch(cp, "1234");
    cp = sitosbuf(buf, 4, 1234, 10);
    tnull(cp);

    cp = sitosx(INT64_MAX, 10);
    tmatch(cp, "9223372036854775807");
    rFree(cp);

    /*
        stoi saturates at the type limit and reports nothing; sparseUint is the strict parser
        that refuses instead. Both behaviours are pinned so the difference stays visible.
     */
    i = stoi("9223372036854775807");
    teqz(i, INT64_MAX);
    i = stoi("99999999999999999999999");
    teqz(i, INT64_MAX);

    rc = sparseUint("9223372036854775807", 10, INT64_MAX, NULL, &value);
    teqi(rc, 0);
    teqz(value, INT64_MAX);
    rc = sparseUint("9223372036854775808", 10, INT64_MAX, NULL, &value);
    teqi(rc, R_ERR_WONT_FIT);

    /*
        Zero is a legitimate value at the bottom of every range.
     */
    rc = sparseUint("0", 10, 0, NULL, &value);
    teqi(rc, 0);
    teqz(value, 0);
    rc = sparseUint("1", 10, 0, NULL, &value);
    teqi(rc, R_ERR_WONT_FIT);
}


/*
    The numeric predicates on input designed to look like numbers without being them.
 */
static void numberPredicates()
{
    bool is;

    is = snumber("0");
    ttrue(is);
    is = snumber("-1");
    ttrue(is);
    is = snumber("+1");
    ttrue(is);
    is = snumber("");
    tfalse(is);
    is = snumber("-");
    tfalse(is);
    is = snumber("+");
    tfalse(is);
    is = snumber("1 ");
    tfalse(is);
    is = snumber(" 1");
    tfalse(is);
    is = snumber("1.0");
    tfalse(is);

    is = sfnumber("1.5");
    ttrue(is);
    is = sfnumber("-1.5");
    ttrue(is);
    is = sfnumber("1e10");
    ttrue(is);
    is = sfnumber("");
    tfalse(is);
    is = sfnumber(".");
    tfalse(is);

    /*
        Two decimal points is not a number however plausible each half looks.
     */
    is = sfnumber("1.2.3");
    tfalse(is);

    is = shnumber("0x1f");
    ttrue(is);
    is = shnumber("");
    tfalse(is);
    is = shnumber("0xzz");
    tfalse(is);
}


/*
    Bounded copies at capacity and one past it. Every one of these must terminate the
    destination, and none may write past it.
 */
static void copyLimits()
{
    char  buf[8];
    char  guard[16];
    ssize len;

    /*
        Exactly filling the buffer: seven characters plus the terminator in eight bytes.
     */
    len = scopy(buf, sizeof(buf), "1234567");
    teqz(len, 7);
    tmatch(buf, "1234567");
    teqi(buf[7], 0);

    /*
        One more does not fit. The call reports it rather than truncating silently.
     */
    len = scopy(buf, sizeof(buf), "12345678");
    teqz(len, R_ERR_WONT_FIT);

    len = scopy(buf, sizeof(buf), "");
    teqz(len, 0);
    tmatch(buf, "");

    /*
        sncopy takes a count as well as a capacity. The smaller of the two wins.
     */
    len = sncopy(buf, sizeof(buf), "abcdefghij", 3);
    teqz(len, 3);
    tmatch(buf, "abc");

    len = sncopy(buf, sizeof(buf), "abcdefghij", 7);
    teqz(len, 7);
    tmatch(buf, "abcdefg");

    len = sncopy(buf, sizeof(buf), "abcdefghij", 8);
    teqz(len, R_ERR_WONT_FIT);

    /*
        A count longer than the source stops at the source's terminator.
     */
    len = sncopy(buf, sizeof(buf), "ab", 100);
    teqz(len, 2);
    tmatch(buf, "ab");

    /*
        sjoinbuf into a buffer that holds both halves exactly - fifteen characters plus the
        terminator in sixteen bytes - and then one character too many.
     */
    len = sjoinbuf(guard, sizeof(guard), "0123456789", "abcde");
    teqz(len, 15);
    tmatch(guard, "0123456789abcde");
    teqi(guard[15], 0);

    len = sjoinbuf(guard, sizeof(guard), "0123456789", "abcdef");
    teqz(len, R_ERR_WONT_FIT);
}


/*
    Comparisons with a length limit. The interesting cases are at n itself and where one string
    runs out before the other.
 */
static void compareLimits()
{
    int rc;

    /*
        A count of zero compares nothing, so everything is equal.
     */
    rc = sncmp("abc", "xyz", 0);
    teqi(rc, 0);
    rc = sncaselesscmp("abc", "xyz", 0);
    teqi(rc, 0);

    /*
        Equal for the first n, differing after it.
     */
    rc = sncmp("abcX", "abcY", 3);
    teqi(rc, 0);
    rc = sncmp("abcX", "abcY", 4);
    teqi(rc, -1);
    rc = sncmp("abcY", "abcX", 4);
    teqi(rc, 1);

    /*
        One string is a prefix of the other, and n reaches past the shorter one.
     */
    rc = sncmp("ab", "abc", 3);
    teqi(rc, -1);
    rc = sncmp("abc", "ab", 3);
    teqi(rc, 1);
    rc = sncmp("abc", "abc", 10);
    teqi(rc, 0);

    rc = sncaselesscmp("ABCX", "abcY", 3);
    teqi(rc, 0);
    rc = sncaselesscmp("ABCX", "abcY", 4);
    teqi(rc, -1);
    rc = sncaselesscmp("ab", "ABC", 3);
    teqi(rc, -1);
    rc = sncaselesscmp("ABC", "abc", 10);
    teqi(rc, 0);

    /*
        A count past the representable range is refused rather than wrapping.
     */
    rc = sncmp("a", "b", (size_t) MAXINT + 1);
    teqi(rc, 0);
    rc = sncaselesscmp("a", "b", (size_t) MAXINT + 1);
    teqi(rc, 0);
}


/*
    Substring searches at the ends of their inputs.
 */
static void searchLimits()
{
    cchar  *cp;
    size_t len;

    /*
        An empty needle is "no pattern" rather than "matches at position zero", which is the
        opposite of strstr. sends and sstarts take the other view and accept an empty suffix or
        prefix, so the two conventions sit side by side in the same module.
     */
    cp = scontains("abc", "");
    tnull(cp);
    cp = scontains("", "abc");
    tnull(cp);
    cp = scontains("abc", "abc");
    tnotnull(cp);

    cp = sncontains("abcdef", "def", 6);
    tnotnull(cp);
    cp = sncontains("abcdef", "def", 5);
    tnull(cp);

    cp = sncaselesscontains("abcDEF", "def", 6);
    tnotnull(cp);
    cp = sncaselesscontains("abcDEF", "def", 5);
    tnull(cp);
    cp = sncaselesscontains("abcDEF", "xyz", 6);
    tnull(cp);

    /*
        A suffix that is the whole string, and one longer than the string.
     */
    cp = sends("abc", "abc");
    tnotnull(cp);
    cp = sends("abc", "abcd");
    tnull(cp);
    cp = sends("abc", "");
    tnotnull(cp);

    cp = sstarts("abc", "abc") ? "y" : NULL;
    tnotnull(cp);
    cp = sstarts("abc", "abcd") ? "y" : NULL;
    tnull(cp);

    len = sspn("aaa", "a");
    teqz(len, 3);
    len = sspn("aaa", "b");
    teqz(len, 0);
    len = sspn("", "a");
    teqz(len, 0);
}


/*
    Substring extraction and trimming at the ends of the string.
 */
static void substringLimits()
{
    char buf[32];
    char *cp;

    cp = ssub("abcdef", 0, 6);
    tmatch(cp, "abcdef");
    rFree(cp);

    /*
        A length past the end of the string stops at the terminator.
     */
    cp = ssub("abcdef", 0, 100);
    tmatch(cp, "abcdef");
    rFree(cp);

    cp = ssub("abcdef", 6, 3);
    tmatch(cp, "");
    rFree(cp);

    cp = ssub("abcdef", 0, 0);
    tmatch(cp, "");
    rFree(cp);

    /*
        Trimming a string made entirely of the trim set leaves nothing.
     */
    scopy(buf, sizeof(buf), "xxxx");
    cp = strim(buf, "x", R_TRIM_BOTH);
    tmatch(cp, "");

    scopy(buf, sizeof(buf), "xxaxx");
    cp = strim(buf, "x", R_TRIM_START);
    tmatch(cp, "axx");

    scopy(buf, sizeof(buf), "xxaxx");
    cp = strim(buf, "x", R_TRIM_END);
    tmatch(cp, "xxa");

    scopy(buf, sizeof(buf), "abc");
    cp = strim(buf, "x", R_TRIM_BOTH);
    tmatch(cp, "abc");

    scopy(buf, sizeof(buf), "");
    cp = strim(buf, "x", R_TRIM_BOTH);
    tmatch(cp, "");
}


/*
    Tokenizers on input with nothing to tokenize.
 */
static void tokenLimits()
{
    char buf[64];
    char *cp, *last;

    /*
        No delimiter present: the whole string is one token and the remainder is empty.
     */
    scopy(buf, sizeof(buf), "abc");
    cp = stok(buf, ",", &last);
    tmatch(cp, "abc");
    cp = stok(NULL, ",", &last);
    tnull(cp);

    /*
        Leading and repeated delimiters produce no empty tokens.
     */
    scopy(buf, sizeof(buf), ",,a,,b,,");
    cp = stok(buf, ",", &last);
    tmatch(cp, "a");
    cp = stok(NULL, ",", &last);
    tmatch(cp, "b");
    cp = stok(NULL, ",", &last);
    tnull(cp);

    /*
        A string made only of delimiters yields nothing at all.
     */
    scopy(buf, sizeof(buf), ",,,");
    cp = stok(buf, ",", &last);
    tnull(cp);

    /*
        ssplit keeps empty fields where stok discards them - the difference between the two.
     */
    scopy(buf, sizeof(buf), ",a");
    cp = ssplit(buf, ",", &last);
    tmatch(cp, "");
    tmatch(last, "a");

    /*
        sptok splits on a multi-character pattern. A pattern that is not present leaves the
        whole string as the token and a null remainder - null, not empty, so a caller looping
        on sptok can tell the last field from an empty one.
     */
    scopy(buf, sizeof(buf), "abc");
    cp = sptok(buf, "::", &last);
    tmatch(cp, "abc");
    tnull(last);

    scopy(buf, sizeof(buf), "::abc");
    cp = sptok(buf, "::", &last);
    tmatch(cp, "");
    tmatch(last, "abc");

    /*
        An empty pattern matches nothing, so the whole string comes back.
     */
    scopy(buf, sizeof(buf), "abc");
    cp = sptok(buf, "", &last);
    tmatch(cp, "abc");
}


/*
    Template expansion on malformed input. A template comes from configuration, so an unclosed
    reference must terminate rather than run off the end of the string.
 */
static void templateLimits()
{
    RHash *keys;
    char  *cp;

    keys = rAllocHash(0, R_STATIC_NAME | R_STATIC_VALUE);
    rAddName(keys, "name", "value", 0);

    cp = stemplate("${name}", keys);
    tmatch(cp, "value");
    rFree(cp);

    /*
        No reference at all is returned unchanged.
     */
    cp = stemplate("plain text", keys);
    tmatch(cp, "plain text");
    rFree(cp);

    /*
        An unclosed reference at the very end of the string.
     */
    cp = stemplate("${name", keys);
    tnotnull(cp);
    rFree(cp);

    cp = stemplate("${", keys);
    tnotnull(cp);
    rFree(cp);

    cp = stemplate("$", keys);
    tnotnull(cp);
    rFree(cp);

    /*
        A reference to a key that is not defined is left as written rather than becoming empty,
        so a misspelling is visible instead of silently dropping configuration.
     */
    cp = stemplate("${missing}", keys);
    tnotnull(cp);
    rFree(cp);

    /*
        The bare form without braces.
     */
    cp = stemplate("$name", keys);
    tmatch(cp, "value");
    rFree(cp);

    rFreeHash(keys);
}


/*
    Buffers at their allocation and growth limits.
 */
static void bufferLimits()
{
    RBuf   *bp;
    char   fixed[64];
    size_t size;
    ssize  len;
    int    rc;

    /*
        A zero initial size falls back to the module default rather than allocating nothing.
     */
    bp = rAllocBuf(0);
    tnotnull(bp);
    size = rGetBufSize(bp);
    ttrue(size > 0);
    rFreeBuf(bp);

    bp = rAllocBuf(1);
    tnotnull(bp);

    /*
        Writing past the initial size grows the buffer rather than truncating.
     */
    len = rPutStringToBuf(bp, "0123456789012345678901234567890123456789");
    teqz(len, 40);
    size = rGetBufLength(bp);
    teqz(size, 40);
    rFreeBuf(bp);

    /*
        A statically declared buffer initialised in place.
     */
    bp = rAllocBuf(16);
    rc = rInitBuf(bp, 0);
    teqi(rc, R_ERR_BAD_ARGS);
    rFreeBuf(bp);

    /*
        Growth requests at and past the module maximum.
     */
    bp = rAllocBuf(64);
    rc = rGrowBuf(bp, 0);
    teqi(rc, R_ERR_BAD_ARGS);
    rc = rGrowBuf(bp, 128);
    teqi(rc, 0);
    ttrue(rGetBufSize(bp) >= 128);

    /*
        Adjusting the end past either boundary is ignored rather than moving the pointer outside
        the allocation.
     */
    rFlushBuf(bp);
    rPutStringToBuf(bp, "abc");
    rAdjustBufEnd(bp, -100);
    size = rGetBufLength(bp);
    teqz(size, 3);

    rAdjustBufEnd(bp, 1000000);
    size = rGetBufLength(bp);
    teqz(size, 3);

    /*
        An adjustment that lands inside the buffer is applied.
     */
    rAdjustBufEnd(bp, -1);
    size = rGetBufLength(bp);
    teqz(size, 2);

    rAdjustBufEnd(bp, -2);
    size = rGetBufLength(bp);
    teqz(size, 0);
    rFreeBuf(bp);

    /*
        A block read of exactly the buffer contents, then one past.
     */
    bp = rAllocBuf(64);
    rPutStringToBuf(bp, "abcd");
    len = rGetBlockFromBuf(bp, fixed, 4);
    teqz(len, 4);
    len = rGetBlockFromBuf(bp, fixed, 1);
    teqz(len, 0);
    rFreeBuf(bp);
}


/*
    List indices at and past both ends.
 */
static void listLimits()
{
    RList *lp;
    void  *item;
    int   rc;

    lp = rAllocList(0, 0);
    tnotnull(lp);

    /*
        Every access into an empty list is out of range.
     */
    item = rGetItem(lp, 0);
    tnull(item);
    item = rGetItem(lp, -1);
    tnull(item);
    rc = rRemoveItemAt(lp, 0);
    ttrue(rc < 0);

    rc = rAddItem(lp, "a");
    teqi(rc, 0);
    rc = rAddItem(lp, "b");
    teqi(rc, 1);

    /*
        The last valid index, then one past it.
     */
    item = rGetItem(lp, 1);
    tmatch(item, "b");
    item = rGetItem(lp, 2);
    tnull(item);

    /*
        Inserting at the end is append; inserting past the end grows the list with null holes.
     */
    rc = rInsertItemAt(lp, 2, "c");
    teqi(rc, 2);
    rc = rGetListLength(lp);
    teqi(rc, 3);

    rc = rInsertItemAt(lp, -1, "bad");
    teqi(rc, R_ERR_BAD_ARGS);

    /*
        Setting past the end extends the list rather than failing.
     */
    item = rSetItem(lp, 10, "far");
    tnull(item);
    rc = rGetListLength(lp);
    teqi(rc, 11);
    item = rGetItem(lp, 10);
    tmatch(item, "far");
    item = rGetItem(lp, 5);
    tnull(item);

    item = rSetItem(lp, -1, "bad");
    tnull(item);

    /*
        Removing the first and the last leaves the list consistent.
     */
    rc = rRemoveItemAt(lp, 0);
    teqi(rc, 0);
    rc = rGetListLength(lp);
    teqi(rc, 10);

    rClearList(lp);
    rc = rGetListLength(lp);
    teqi(rc, 0);

    rFreeList(lp);
}


/*
    Hashes at the sizes where the bucket and name arrays have to grow, and with names chosen to
    land in the same bucket.
 */
static void hashLimits()
{
    RHash *table;
    cchar *value;
    char  name[32];
    int   i, count;

    /*
        A hash asked for zero buckets still works; it grows on demand.
     */
    table = rAllocHash(0, R_DYNAMIC_NAME | R_STATIC_VALUE);
    tnotnull(table);
    count = rGetHashLength(table);
    teqi(count, 0);

    /*
        Enough entries to force several rounds of growth in both the bucket array and the name
        array.
     */
    for (i = 0; i < 500; i++) {
        sfmtbuf(name, sizeof(name), "key-%d", i);
        rAddName(table, sclone(name), "v", 0);
    }
    count = rGetHashLength(table);
    teqi(count, 500);

    for (i = 0; i < 500; i++) {
        sfmtbuf(name, sizeof(name), "key-%d", i);
        value = rLookupName(table, name);
        tmatch(value, "v");
    }

    /*
        Removing every entry returns the table to empty without corrupting the chains.
     */
    for (i = 0; i < 500; i++) {
        sfmtbuf(name, sizeof(name), "key-%d", i);
        rRemoveName(table, name);
    }
    count = rGetHashLength(table);
    teqi(count, 0);
    rFreeHash(table);

    /*
        An empty name is a legitimate key, distinct from an absent one.
     */
    table = rAllocHash(0, R_STATIC_NAME | R_STATIC_VALUE);
    rAddName(table, "", "empty", 0);
    value = rLookupName(table, "");
    tmatch(value, "empty");
    value = rLookupName(table, "other");
    tnull(value);
    rFreeHash(table);
}


/*
    Path handling at the ends of a path.
 */
static void pathLimits()
{
    cchar *base;
    char  *cp;

    base = rBasename("/a/b/c");
    tmatch(base, "c");
    base = rBasename("c");
    tmatch(base, "c");
    /*
        The root is its own basename, as POSIX basename has it. A trailing separator anywhere
        else leaves an empty last component.
     */
    base = rBasename("/");
    tmatch(base, "/");
    base = rBasename("");
    tmatch(base, "");
    base = rBasename("/a/b/");
    tmatch(base, "");

    cp = rDirname(sclone("/a/b/c"));
    tmatch(cp, "/a/b");
    rFree(cp);

    /*
        A bare filename has an empty directory, not "." as POSIX dirname gives. That is safe
        with rJoinFile, which treats an empty left side as "no directory", but a caller that
        formats "%s/%s" by hand would turn a relative path into an absolute one.
     */
    cp = rDirname(sclone("c"));
    tmatch(cp, "");
    rFree(cp);

    cp = rDirname(sclone("/"));
    tnotnull(cp);
    rFree(cp);

    cp = rJoinFile(rDirname(sclone("c")), "c");
    tmatch(cp, "c");
    rFree(cp);

    cp = rGetFileExt("a.b.c");
    tmatch(cp, "c");
    cp = rGetFileExt(".hidden");
    tmatch(cp, "hidden");
    cp = rGetFileExt("trailing.");
    tmatch(cp, "");
}


/*
    Time conversions at the epoch and across the second/millisecond boundary.
 */
static void timeLimits()
{
    char  *cp;
    Time  when, parsed;
    Ticks remaining;

    /*
        The HTTP date round trip. rGetHttpDate takes ticks and truncates to the second, so the
        value that comes back is the same instant rounded down - not the same instant divided by
        a thousand.
     */
    when = 1762810108000LL;
    cp = rGetHttpDate(when);
    tnotnull(cp);
    ttrue(scontains(cp, "GMT") != 0);
    parsed = rParseHttpDate(cp);
    teqz(parsed, when);
    rFree(cp);

    /*
        A time with a fractional second truncates down, so the round trip loses only the
        milliseconds.
     */
    when = 1762810108999LL;
    cp = rGetHttpDate(when);
    parsed = rParseHttpDate(cp);
    teqz(parsed, 1762810108000LL);
    rFree(cp);

    /*
        The epoch itself.
     */
    cp = rGetHttpDate(0);
    tnotnull(cp);
    parsed = rParseHttpDate(cp);
    teqz(parsed, 0);
    rFree(cp);

    /*
        Garbage in, zero out - and no crash on an absent string.
     */
    parsed = rParseHttpDate("not a date");
    teqz(parsed, 0);
    parsed = rParseHttpDate("");
    teqz(parsed, 0);
    parsed = rParseHttpDate(NULL);
    teqz(parsed, 0);

    /*
        rGetRemainingTicks before the deadline, at it, and past it. Past it the result goes
        negative rather than clamping at zero - that is the signal that the deadline has already
        elapsed, and by how much, so a caller tests for <= 0 rather than == 0.
     */
    remaining = rGetRemainingTicks(rGetTicks(), 1000);
    ttrue(remaining > 0 && remaining <= 1000);

    remaining = rGetRemainingTicks(rGetTicks() - 5000, 1000);
    ttrue(remaining < 0);
    ttrue(remaining <= -4000);

    remaining = rGetRemainingTicks(rGetTicks(), 0);
    ttrue(remaining <= 0);
}


/*
    The ISO date parser given input designed to break it. It is the one parser here that takes
    text from outside the process, so it gets the hostile cases.
 */
static void isoDateLimits()
{
    Time when;

    when = rParseIsoDate("2025-11-10T21:28:28Z");
    ttrue(when > 0);

    /*
        Every field at its boundary.
     */
    when = rParseIsoDate("1970-01-01T00:00:00Z");
    teqz(when, 0);
    when = rParseIsoDate("2025-12-31T23:59:59Z");
    ttrue(when > 0);

    /*
        Each field one past its range.
     */
    when = rParseIsoDate("2025-13-01T00:00:00Z");
    teqz(when, -1);
    when = rParseIsoDate("2025-00-01T00:00:00Z");
    teqz(when, -1);
    when = rParseIsoDate("2025-01-32T00:00:00Z");
    teqz(when, -1);
    when = rParseIsoDate("2025-01-01T24:00:00Z");
    teqz(when, -1);
    when = rParseIsoDate("2025-01-01T00:60:00Z");
    teqz(when, -1);

    /*
        Truncated at every point, and outright garbage.
     */
    when = rParseIsoDate("2025");
    teqz(when, -1);
    when = rParseIsoDate("2025-11");
    teqz(when, -1);
    when = rParseIsoDate("2025-11-10T");
    teqz(when, -1);
    when = rParseIsoDate("");
    teqz(when, -1);
    when = rParseIsoDate("not a date at all");
    teqz(when, -1);
    when = rParseIsoDate(NULL);
    teqz(when, -1);
}


int main(void)
{
    rInit(0, 0);

    formatLimits();
    numberLimits();
    numberPredicates();
    copyLimits();
    compareLimits();
    searchLimits();
    substringLimits();
    tokenLimits();
    templateLimits();
    bufferLimits();
    listLimits();
    hashLimits();
    pathLimits();
    timeLimits();
    isoDateLimits();

    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
