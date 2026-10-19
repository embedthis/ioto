/*
    regress.tst.c - Regression pins for shipped fixes

    One place to see that a specific defect stays fixed. Most of these have a fuller pin in the
    test file for their module - the value of gathering them here is that each one names the
    defect and the commit, so a reviewer can check the list without reading nine files.

    Every pin here MUST fail against the code before its fix - an assertion that would pass
    either way belongs in its module's own test file, not here. All eight of the pins below were
    verified that way: the fixes were stashed, the library rebuilt, and each condition observed
    failing, then observed passing again once the fixes were restored.

    directoryEnumeration is the exception and is marked as such: its fix is Windows-only and
    already shipped, so on any other platform it guards the invariant rather than reproducing the
    defect.

    The TestMe comparison macros expand their arguments twice, so every call with a side effect
    is bound to a local before it is asserted.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "r.h"

/*********************************** Locals ***********************************/

static int walkCount;

/************************************ Code ************************************/

/*
    %X promoted its argument to 64 bits on a 64-bit build, so it read eight bytes of varargs for
    a caller that had pushed a four-byte int. Any value with the top bit set printed garbage,
    while %x of the same value was correct. Full pin in edge.tst.c.
 */
static void hexFormatWidth()
{
    char *cp;

    cp = sfmt("%X", 0xDEADBEEF);
    tmatch(cp, "DEADBEEF");
    rFree(cp);

    cp = sfmt("%x", 0xDEADBEEF);
    tmatch(cp, "deadbeef");
    rFree(cp);
}


/*
    outNum takes an int64, and the unsigned conversions passed their value straight into it. A
    value above INT64_MAX arrived with its top bit set and was printed as a negative number, so
    "%llx" of an all-ones 64-bit value produced "-1". Full pin in edge.tst.c.
 */
static void unsignedFormatSign()
{
    char *cp;

    cp = sfmt("%llx", (uint64) 0xFFFFFFFFFFFFFFFFULL);
    tmatch(cp, "ffffffffffffffff");
    rFree(cp);

    cp = sfmt("%llu", (uint64) 0xFFFFFFFFFFFFFFFFULL);
    tmatch(cp, "18446744073709551615");
    rFree(cp);
}


/*
    rGetHttpDate divided its argument by TPS and then passed the result to universalTime, which
    divides by TPS itself. Every HTTP date it produced was a thousand times too small - a date in
    January 1970 for any real timestamp. rParseHttpDate returned seconds where its contract and
    its sibling rParseIsoDate return ticks. Full pin in edge.tst.c.
 */
static void httpDateUnits()
{
    char *cp;
    Time parsed;

    /*
        A timestamp from 2025, not from 1970.
     */
    cp = rGetHttpDate(1762810108000LL);
    tnotnull(cp);
    tmatch(cp, "Mon, 10 Nov 2025 21:28:28 GMT");

    /*
        And the round trip returns the same instant, not that instant divided by a thousand.
     */
    parsed = rParseHttpDate(cp);
    teqz(parsed, 1762810108000LL);
    rFree(cp);
}


/*
    sparseUint is the strict wire-format parser: its whole purpose is to reject a value outside
    the caller's range. Its overflow guard divides, and integer division truncates to zero for a
    small ceiling, so a first digit larger than the ceiling escaped the check entirely and was
    accepted. Full pin in string.tst.c.
 */
static void sparseUintCeiling()
{
    int64 value;
    int   rc;

    rc = sparseUint("9", 10, 5, NULL, &value);
    teqi(rc, R_ERR_WONT_FIT);
    teqz(value, 0);

    rc = sparseUint("1", 10, 0, NULL, &value);
    teqi(rc, R_ERR_WONT_FIT);

    /*
        The value at the ceiling is still accepted.
     */
    rc = sparseUint("5", 10, 5, NULL, &value);
    teqi(rc, 0);
    teqz(value, 5);
}


/*
    sjoinbuf cast sncopy's signed return to size_t without testing it. A first copy that did not
    fit produced a huge length, which then formed an out of bounds &buf[len] and returned a
    meaningless positive result - the shape that turns into a buffer overrun one call downstream.
    Full pin in edge.tst.c.
 */
static void joinBufferError()
{
    char  buf[16];
    ssize len;

    len = sjoinbuf(buf, sizeof(buf), "0123456789", "abcdef");
    teqz(len, R_ERR_WONT_FIT);

    /*
        Exactly filling the buffer still succeeds and reports the real length.
     */
    len = sjoinbuf(buf, sizeof(buf), "0123456789", "abcde");
    teqz(len, 15);
    tmatch(buf, "0123456789abcde");
}


/*
    The rGetBuf macro read (bp)->data and RBuf has no "data" member, so every caller of the
    documented API failed to compile. This function compiling at all is most of the pin.
    Full pin in buf.tst.c.
 */
static void bufAccessor()
{
    RBuf *bp;
    char *base;

    bp = rAllocBuf(64);
    tnotnull(bp);
    rPutStringToBuf(bp, "abc");

    base = rGetBuf(bp);
    tnotnull(base);
    ttrue(base == rGetBufStart(bp));
    rFreeBuf(bp);
}


/*
    scaselesscmp and sncaselesscmp tested s1 first and reported null as less than anything,
    including another null, while scmp and sncmp treated two nulls as equal. scaselessmatch was
    therefore false for two absent strings where smatch was true. Full pin in null.tst.c.
 */
static void caselessNullSymmetry()
{
    bool match;
    int  rc;

    rc = scaselesscmp(NULL, NULL);
    teqi(rc, 0);
    rc = sncaselesscmp(NULL, NULL, 3);
    teqi(rc, 0);

    match = scaselessmatch(NULL, NULL);
    ttrue(match);

    /*
        The two families agree.
     */
    rc = scmp(NULL, NULL);
    teqi(rc, 0);
    match = smatch(NULL, NULL);
    ttrue(match);
}


/*
    rRemoveName returned 0 for a null table or name - the same value it returns for a successful
    removal - so the caller could not tell "removed" from "never asked". Full pin in null.tst.c.
 */
static void removeNameReturn()
{
    RHash *table;
    int   rc;

    table = rAllocHash(0, R_STATIC_NAME | R_STATIC_VALUE);
    rAddName(table, "present", "v", 0);

    rc = rRemoveName(NULL, "present");
    teqi(rc, R_ERR_BAD_ARGS);
    rc = rRemoveName(table, NULL);
    teqi(rc, R_ERR_BAD_ARGS);

    /*
        A real removal is still zero, and a name that is not there is still CANT_FIND.
     */
    rc = rRemoveName(table, "present");
    teqi(rc, 0);
    rc = rRemoveName(table, "absent");
    teqi(rc, R_ERR_CANT_FIND);

    rFreeHash(table);
}


static int countCallback(void *arg, cchar *path, int flags)
{
    walkCount++;
    return 0;
}


/*
    418f6d2: openDirList stored the FindFirstFile result in a local that was lost on return, and
    getNextFile started from FindNextFile, so the first entry of every directory was skipped on
    Windows. The consequence is Windows-only but the invariant is not, and a count is the only
    thing that catches an off-by-one in an enumeration: the fixture below has a known number of
    entries and all of them must come back.
 */
static void directoryEnumeration()
{
    char dir[ME_MAX_FNAME], path[ME_MAX_FNAME];
    int  i, rc;

    sfmtbuf(dir, sizeof(dir), "data/temp-regress-%d", (int) getpid());
    mkdir(dir, 0755);

    for (i = 0; i < 5; i++) {
        sfmtbuf(path, sizeof(path), "%s/entry-%d.dat", dir, i);
        rWriteFile(path, "x", 1, 0644);
    }

    walkCount = 0;
    rc = rWalkDir(dir, "*.dat", countCallback, NULL, R_WALK_FILES);
    teqi(rc, 5);
    teqi(walkCount, 5);

    /*
        And with a single entry, where losing the first loses everything.
     */
    for (i = 1; i < 5; i++) {
        sfmtbuf(path, sizeof(path), "%s/entry-%d.dat", dir, i);
        unlink(path);
    }
    walkCount = 0;
    rc = rWalkDir(dir, "*.dat", countCallback, NULL, R_WALK_FILES);
    teqi(rc, 1);
    teqi(walkCount, 1);

    sfmtbuf(path, sizeof(path), "%s/entry-0.dat", dir);
    unlink(path);
    rmdir(dir);
}


/*
    rInsertItemAt(lp, INT_MAX, item) computed "index + 1" for the required capacity, which
    wrapped to INT_MIN. rGrowList read the negative size as "already large enough" and returned
    success without growing, and the store then wrote to lp->items[INT_MAX]. Aborted before the fix.
 */
static void insertItemBound()
{
    RList *lp;
    int   rc;

    lp = rAllocList(0, 0);
    rc = rInsertItemAt(lp, INT_MAX, "item");
    teqi(rc, R_ERR_BAD_ARGS);
    teqi(rGetListLength(lp), 0);
    rFreeList(lp);
}

/*
    A negative "*" width was negated, but negating INT_MIN is undefined and leaves the
    width negative. In outNum the "fill < 0" clamp then never fired; in outString the width was
    cast to size_t and became ~2^63. Either way the pad loop ran long enough to stall the whole
    single-threaded event loop - 9 seconds for %d, unbounded for %s. The pin is the elapsed time:
    both of these took minutes or never returned before the fix.
 */
static void extremePadWidth()
{
    Ticks elapsed, mark;
    char  buf[64];
    ssize rc;

    mark = rGetTicks();
    rc = rSnprintf(buf, sizeof(buf), "%*d", INT_MIN, 5);
    elapsed = rGetTicks() - mark;
    ttrue(elapsed < 2000);
    ttrue(rc >= 0);
    ttrue(slen(buf) < sizeof(buf));

    mark = rGetTicks();
    rc = rSnprintf(buf, sizeof(buf), "%*s", INT_MIN, "x");
    elapsed = rGetTicks() - mark;
    ttrue(elapsed < 2000);
    ttrue(rc >= 0);
    ttrue(slen(buf) < sizeof(buf));

    //  An ordinary negative width still left-aligns
    rc = rSnprintf(buf, sizeof(buf), "%*d|", -6, 42);
    teqz(rc, 7);
    tmatch(buf, "42    |");
}

/*
    rJoinFileBuf reserved one byte where the separator path needs two, so a buffer that
    was exactly one short passed the check, took the truncating scopy, and was returned non-NULL
    with no null terminator.
 */
static void joinFileBufBound()
{
    char buf[5];

    memset(buf, 'Z', sizeof(buf));
    tnull(rJoinFileBuf(buf, sizeof(buf), "ab", "cd"));

    //  One more byte is enough for "ab/cd" plus the null. The join uses the native separator.
    tnotnull(rJoinFileBuf(buf, sizeof(buf), "a", "cd"));
#if ME_WIN_LIKE
    tmatch(buf, "a\\cd");
#else
    tmatch(buf, "a/cd");
#endif
}

/*
    svalue guarded only positive overflow in its suffix multiply, so a negative magnitude
    multiplied straight through and overflowed.
 */
static void valueSuffixOverflow()
{
    teqll(svalue("-9223372036854775808k"), INT64_MIN);
    teqll(svalue("9223372036854775807k"), INT64_MAX);
    teqll(svalue("2k"), 2048);
    teqll(svalue("-2k"), -2048);
}

/*
    ssub never validated offset against the string length, so &str[offset] pointed past
    the end and sncopy scanned it with slen.
 */
static void subStringOffset()
{
    char *sp;

    sp = ssub("abc", 10, 3);
    tmatch(sp, "");
    rFree(sp);

    sp = ssub("abc", 3, 3);
    tmatch(sp, "");
    rFree(sp);

    sp = ssub("abc", 1, 2);
    tmatch(sp, "bc");
    rFree(sp);
}

/*
    outFloatE stripped trailing zeros for %g by walking ctx->end back without also
    decrementing ctx->len, so the reported length exceeded the string actually produced. outFloat
    had the decrement; outFloatE did not.
 */
static void floatExponentLength()
{
    char  buf[64];
    ssize rc;

    rc = rSnprintf(buf, sizeof(buf), "%g", 1.0e20);
    teqz(rc, (ssize) slen(buf));

    rc = rSnprintf(buf, sizeof(buf), "%g", 1.5e-20);
    teqz(rc, (ssize) slen(buf));

    rc = rSnprintf(buf, sizeof(buf), "%e", 1.0e20);
    teqz(rc, (ssize) slen(buf));
}

/*
    rPutIntToBuf dereferenced a null buffer, and rAddIntName formatted into 16 bytes -
    two short of INT64_MIN - so sitosbuf returned null and the name was silently stored as "".
 */
static void nullAndWidthGuards()
{
    RHash *hash;
    cchar *value;

    teqz(rPutIntToBuf(NULL, 42), R_ERR_BAD_ARGS);

    hash = rAllocHash(0, 0);
    rAddIntName(hash, "min", INT64_MIN);
    value = rLookupName(hash, "min");
    tmatch(value, "-9223372036854775808");
    rFreeHash(hash);
}


int main(void)
{
    rInit(0, 0);

    hexFormatWidth();
    unsignedFormatSign();
    httpDateUnits();
    sparseUintCeiling();
    joinBufferError();
    bufAccessor();
    caselessNullSymmetry();
    removeNameReturn();
    directoryEnumeration();
    insertItemBound();
    extremePadWidth();
    joinFileBufBound();
    valueSuffixOverflow();
    subStringOffset();
    floatExponentLength();
    nullAndWidthGuards();

    rTerm();
    return 0;
}

/*
    Not pinned here, and why:

    rRunEvent relink                 - needs the event loop, so its pin lives in event.tst.c
                                       where the fiber and loop are already running.
    8c4cda9, CRL enforcement         - needs a revoked certificate and a live handshake, so it
                                       is pinned in tls-verify.tst.c instead. test/prep.sh now
                                       builds the revoked certificate and the two CRLs.
    acbea21, peer verification       - pinned by tls-verify.tst.c, which shipped with the fix.
    67c712a, printf wrapper          - PRINTF_ATTRIBUTE is a compile-time check. The pin is that
    attribution                        the build stays warning-free, not a runtime assertion.
    rFreeSocket reentrancy           - needs a fiber blocked in rWaitForIO on the socket being
                                       freed. Reproducing it means driving the event loop from
                                       two fibers; socket.tst.c is where that machinery lives.
    TLS key and handshake states     - loadKey, handshake and rConfigTls all need a live
                                       handshake. tls-verify.tst.c covers the observable
                                       behaviour; the internal states here are not reachable
                                       from a test without a malformed key or a peer that
                                       aborts mid-handshake.
    errno collision                  - the return values only differ on a socket error that is
                                       neither EAGAIN nor ECONNRESET. Provoking one reliably
                                       needs a peer under test control.
    rSendFile deadline               - a signature change. The pin is that every caller passes a
                                       deadline and the build stays clean.

    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
