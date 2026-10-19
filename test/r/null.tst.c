/*
    null.tst.c - The null tolerance contract

    CLAUDE.md states that "most functions are null tolerant". Most is not a specification, so
    this file asserts tolerance for the functions that have it and names the ones that do not in
    the exceptions table at the end.

    Nothing here calls a function known to dereference a null. A null tolerance failure kills the
    process rather than failing an assertion, taking the whole suite with it, so a candidate is
    read in src/ before it is added.

    Several functions guard with assert(), which in a debug library logs through the error path
    rather than aborting. A counting log handler is installed so those messages are asserted
    rather than printed.

    The TestMe comparison macros expand their arguments twice, so every call with a side effect
    is bound to a local before it is asserted.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "r.h"

/*********************************** Locals ***********************************/

static cchar *currentGroup;            /* Raise this in a debugger if the file ever crashes */
static int   logged;

/************************************ Code ************************************/

static void quietHandler(cchar *type, cchar *source, cchar *msg)
{
    logged++;
}


static void group(cchar *name)
{
    currentGroup = name;
}


/*
    Clone-like functions return an empty string rather than null so a caller can chain without
    checking. scloneNull and scloneDefined are the deliberate exceptions.
 */
static void nullClones()
{
    char *cp;

    group("clone");

    cp = sclone(NULL);
    tmatch(cp, "");
    rFree(cp);

    cp = snclone(NULL, 5);
    tmatch(cp, "");
    rFree(cp);

    cp = scamel(NULL);
    tmatch(cp, "");
    rFree(cp);

    cp = stitle(NULL);
    tmatch(cp, "");
    rFree(cp);

    cp = scloneNull(NULL);
    tnull(cp);

    cp = scloneDefined(NULL);
    tnull(cp);
}


/*
    Comparisons order null before any string and treat two nulls as equal.
 */
static void nullCompares()
{
    int  rc;
    bool match;

    group("compare");

    rc = scmp(NULL, NULL);
    teqi(rc, 0);
    rc = scmp(NULL, "a");
    teqi(rc, -1);
    rc = scmp("a", NULL);
    teqi(rc, 1);

    /*
        The caseless comparisons follow the same rule, so a caller can use scmp and
        scaselesscmp interchangeably and get the same answer for two absent strings.
     */
    rc = scaselesscmp(NULL, NULL);
    teqi(rc, 0);
    rc = scaselesscmp(NULL, "a");
    teqi(rc, -1);
    rc = scaselesscmp("a", NULL);
    teqi(rc, 1);

    rc = sncmp(NULL, NULL, 3);
    teqi(rc, 0);
    rc = sncaselesscmp(NULL, NULL, 3);
    teqi(rc, 0);

    match = smatch(NULL, NULL);
    ttrue(match);
    match = smatch(NULL, "a");
    tfalse(match);

    match = smatchsec(NULL, NULL);
    ttrue(match);
    match = smatchsec(NULL, "a");
    tfalse(match);

    /*
        scaselessmatch follows scaselesscmp, so it agrees with smatch on two absent strings.
     */
    match = scaselessmatch(NULL, NULL);
    ttrue(match);
    match = scaselessmatch(NULL, "a");
    tfalse(match);
    match = scaselessmatch("A", "a");
    ttrue(match);
}


/*
    Searches return null rather than dereferencing either argument.
 */
static void nullSearches()
{
    cchar *cp;
    size_t len;
    bool   starts;

    group("search");

    len = slen(NULL);
    teqz(len, 0);

    cp = schr(NULL, 'a');
    tnull(cp);
    cp = srchr(NULL, 'a');
    tnull(cp);
    cp = spbrk(NULL, "a");
    tnull(cp);
    cp = spbrk("a", NULL);
    tnull(cp);

    cp = scontains(NULL, "a");
    tnull(cp);
    cp = scontains("a", NULL);
    tnull(cp);
    cp = sncontains(NULL, NULL, 0);
    tnull(cp);
    cp = sncaselesscontains(NULL, NULL, 0);
    tnull(cp);

    cp = sends(NULL, "a");
    tnull(cp);
    cp = sends("a", NULL);
    tnull(cp);

    starts = sstarts(NULL, "a");
    tfalse(starts);
    starts = sstarts("a", NULL);
    tfalse(starts);

    len = sspn(NULL, "a");
    teqz(len, 0);
    len = sspn("a", NULL);
    teqz(len, 0);
}


/*
    Predicates and conversions. sspace is the odd one out: a null string is "all whitespace".
 */
static void nullPredicates()
{
    double d;
    int64  i;
    bool   is;

    group("predicate");

    is = snumber(NULL);
    tfalse(is);
    is = shnumber(NULL);
    tfalse(is);
    is = sfnumber(NULL);
    tfalse(is);

    is = sspace(NULL);
    ttrue(is);

    i = stoi(NULL);
    teqz(i, 0);
    i = stoix(NULL, NULL, 10);
    teqz(i, 0);
    i = svalue(NULL);
    teqz(i, 0);

    d = stof(NULL);
    ttrue(d == 0.0);

    /*
        stod is the exception among the numeric parsers: a null string is not-a-number rather
        than zero, so a caller can tell "absent" from "zero".
     */
    d = stod(NULL);
    ttrue(d != d);
}


/*
    Bounded copies refuse a null destination or source rather than writing through it.
 */
static void nullCopies()
{
    char  buf[32];
    ssize len;

    group("copy");

    len = scopy(NULL, sizeof(buf), "abc");
    teqz(len, R_ERR_BAD_ARGS);
    len = scopy(buf, 0, "abc");
    teqz(len, R_ERR_BAD_ARGS);

    /*
        A null source copies nothing but still terminates the destination.
     */
    scopy(buf, sizeof(buf), "x");
    len = scopy(buf, sizeof(buf), NULL);
    teqz(len, 0);
    tmatch(buf, "");

    len = sncopy(NULL, sizeof(buf), "abc", 3);
    teqz(len, R_ERR_BAD_ARGS);
    len = sncopy(buf, sizeof(buf), NULL, 3);
    teqz(len, R_ERR_BAD_ARGS);

    len = sncat(NULL, sizeof(buf), "abc");
    teqz(len, R_ERR_BAD_ARGS);
    len = sncat(buf, sizeof(buf), NULL);
    teqz(len, R_ERR_BAD_ARGS);
}


/*
    In-place string operations. slower/supper pass null straight through, which is the opposite
    convention to the cloners above.
 */
static void nullMutators()
{
    char *cp;

    group("mutate");

    cp = slower(NULL);
    tnull(cp);
    cp = supper(NULL);
    tnull(cp);

    cp = strim(NULL, " ", R_TRIM_BOTH);
    tnull(cp);
    cp = strim("a", NULL, R_TRIM_BOTH);
    tnull(cp);

    cp = stok(NULL, NULL, NULL);
    tnull(cp);

    cp = sptok(NULL, NULL, NULL);
    tnull(cp);

    /*
        ssplit returns an empty string rather than null, so the caller can keep splitting.
     */
    cp = ssplit(NULL, ",", NULL);
    tmatch(cp, "");

    /*
        szero walks to the terminator, so a null pointer is simply no work.
     */
    szero(NULL);

    cp = ssub(NULL, 0, 3);
    tnull(cp);

    /*
        sreplace with nothing to do clones the input, so a null string yields an allocated empty
        string rather than null. The result is owned by the caller either way.
     */
    cp = sreplace(NULL, "a", "b");
    tmatch(cp, "");
    rFree(cp);
    cp = sreplace("abc", NULL, "b");
    tmatch(cp, "abc");
    rFree(cp);
}


/*
    Formatting. A null format is treated as "%s" and a null %s argument prints "null" rather
    than dereferencing.
 */
static void nullFormats()
{
    char *cp;

    group("format");

    cp = sfmt(NULL);
    tnotnull(cp);
    rFree(cp);

    cp = sfmt("%s", (char*) NULL);
    tmatch(cp, "null");
    rFree(cp);

    cp = sfmt("%p", (void*) NULL);
    tnotnull(cp);
    rFree(cp);

    cp = sjoin(NULL, NULL);
    tnotnull(cp);
    rFree(cp);

    cp = sjoinfmt(NULL, "%s", "x");
    tmatch(cp, "x");
    rFree(cp);

    cp = stemplate(NULL, NULL);
    tmatch(cp, "");
    rFree(cp);
}


/*
    Buffers. The accessors are uniformly tolerant, which is what lets a caller test a buffer it
    never managed to allocate.
 */
static void nullBuffers()
{
    RBuf   *bp, *nullBuf = NULL;
    cchar  *cp;
    char   *sp;
    char   tmp[16];
    size_t size;
    ssize  len;
    int    c, rc;

    group("buffer");

    rFreeBuf(NULL);
    rTermBuf(NULL);
    rFlushBuf(NULL);
    rCompactBuf(NULL);
    rResetBufIfEmpty(NULL);
    rAddNullToBuf(NULL);
    rAdjustBufEnd(NULL, 1);
    rAdjustBufStart(NULL, 1);

    rc = rInitBuf(NULL, 16);
    teqi(rc, R_ERR_BAD_ARGS);

    /*
        The accessors are macros that test the pointer, so they need a typed null: the false
        arm of the conditional still has to name a struct member.
     */
    size = rGetBufLength(nullBuf);
    teqz(size, 0);
    size = rGetBufSize(nullBuf);
    teqz(size, 0);
    size = rGetBufSpace(nullBuf);
    teqz(size, 0);

    cp = rGetBufStart(nullBuf);
    tnull(cp);
    cp = rGetBufEnd(nullBuf);
    tnull(cp);
    sp = rGetBuf(nullBuf);
    tnull(sp);

    c = rGetCharFromBuf(NULL);
    teqi(c, -1);
    c = rLookAtNextCharInBuf(NULL);
    teqi(c, -1);
    c = rLookAtLastCharInBuf(NULL);
    teqi(c, -1);

    len = rGetBlockFromBuf(NULL, tmp, sizeof(tmp));
    ttrue(len <= 0);

    rc = rInserCharToBuf(NULL, 'a');
    teqi(rc, R_ERR_BAD_ARGS);

    len = rPutBlockToBuf(NULL, "abc", 3);
    ttrue(len < 0);
    len = rPutStringToBuf(NULL, "abc");
    ttrue(len < 0);

    /*
        A null string is nothing to append, not an error.
     */
    bp = rAllocBuf(64);
    len = rPutStringToBuf(bp, NULL);
    teqz(len, 0);
    len = rPutSubToBuf(bp, NULL, 3);
    teqz(len, 0);
    rFreeBuf(bp);

    cp = rBufToString(NULL);
    tnull(cp);
    sp = rBufToStringAndFree(NULL);
    tnull(sp);
}


/*
    Lists.
 */
static void nullLists()
{
    RList *lp, *nullList = NULL;
    void  *item;
    char  *sp;
    int   next, rc;

    group("list");

    rFreeList(NULL);
    rClearList(NULL);
    rPushItem(NULL, "a");

    rc = rAddItem(NULL, "a");
    teqi(rc, R_ERR_BAD_ARGS);
    rc = rAddNullItem(NULL);
    teqi(rc, R_ERR_BAD_ARGS);
    rc = rInsertItemAt(NULL, 0, "a");
    teqi(rc, R_ERR_BAD_ARGS);

    rc = rGetListLength(nullList);
    teqi(rc, 0);

    item = rGetItem(NULL, 0);
    tnull(item);
    item = rSetItem(NULL, 0, "a");
    tnull(item);
    item = rPopItem(NULL);
    tnull(item);

    next = 0;
    item = rGetNextItem(NULL, &next);
    tnull(item);
    lp = rAllocList(0, 0);
    item = rGetNextItem(lp, NULL);
    tnull(item);

    rc = rLookupItem(NULL, "a");
    ttrue(rc < 0);
    rc = rLookupStringItem(NULL, "a");
    ttrue(rc < 0);
    rc = rLookupStringItem(lp, NULL);
    ttrue(rc < 0);

    rc = rRemoveItem(NULL, "a");
    ttrue(rc < 0);
    rc = rRemoveItemAt(NULL, 0);
    ttrue(rc < 0);
    rc = rRemoveStringItem(NULL, "a");
    ttrue(rc < 0);
    rc = rRemoveStringItem(lp, NULL);
    ttrue(rc < 0);

    lp = rSortList(NULL, NULL, NULL);
    tnull(lp);

    /*
        A null list renders as an empty string rather than a null one.
     */
    sp = rListToString(NULL, ",");
    tmatch(sp, "");
    rFree(sp);
}


/*
    Hashes.
 */
static void nullHashes()
{
    RHash *hp;
    RName *np;
    RBuf  *bp;
    void  *value;
    char  *sp;
    int   rc;

    group("hash");

    rFreeHash(NULL);

    rc = rGetHashLength(NULL);
    teqi(rc, 0);

    np = rAddName(NULL, "a", "b", 0);
    tnull(np);
    hp = rAllocHash(0, R_STATIC_NAME | R_STATIC_VALUE);
    np = rAddName(hp, NULL, "b", 0);
    tnull(np);

    np = rAddDuplicateName(NULL, "a", "b", 0);
    tnull(np);
    np = rAddDuplicateName(hp, NULL, "b", 0);
    tnull(np);

    value = rLookupName(NULL, "a");
    tnull(value);
    value = rLookupName(hp, NULL);
    tnull(value);
    np = rLookupNameEntry(NULL, "a");
    tnull(np);
    np = rLookupNameEntry(hp, NULL);
    tnull(np);

    /*
        A null table or name is rejected rather than reported as a successful removal, so the
        return distinguishes "removed" from "never asked".
     */
    rc = rRemoveName(NULL, "a");
    teqi(rc, R_ERR_BAD_ARGS);
    rc = rRemoveName(hp, NULL);
    teqi(rc, R_ERR_BAD_ARGS);
    rc = rRemoveName(hp, "absent");
    teqi(rc, R_ERR_CANT_FIND);

    np = rGetNextName(NULL, NULL);
    tnull(np);

    hp = rCloneHash(NULL);
    tnull(hp);

    /*
        The serializers treat an absent hash as an empty one.
     */
    sp = rHashToJson(NULL, 0);
    tmatch(sp, "{}");
    rFree(sp);

    bp = rHashToJsonBuf(NULL, NULL, 0);
    tnull(bp);

    bp = rAllocBuf(64);
    bp = rHashToJsonBuf(NULL, bp, 0);
    tnotnull(bp);
    rFreeBuf(bp);
}


/*
    Filesystem helpers that take a path.
 */
static void nullPaths()
{
    cchar *cp;
    char  *sp;
    ssize size;
    bool  exists;

    group("path");

    exists = rFileExists(NULL);
    tfalse(exists);

    size = rGetFileSize(NULL);
    teqz(size, R_ERR_CANT_FIND);

    cp = rGetFileExt(NULL);
    tnull(cp);

    cp = rBasename(NULL);
    tmatch(cp, "");

    sp = rGetFilePath(NULL);
    tnull(sp);

    sp = rJoinFileBuf(NULL, 16, "a", "b");
    tnull(sp);

    exists = rMatchFile(NULL, "*");
    tfalse(exists);
}


/*
    Free is null tolerant everywhere, which is what makes single-exit cleanup possible.
 */
static void nullFrees()
{
    group("free");

    rFree(NULL);
    rFreeBuf(NULL);
    rFreeList(NULL);
    rFreeHash(NULL);
    rFreeSocket(NULL);

    /*
        Reaching here without a crash is the assertion.
     */
    ttrue(1);
}


int main(void)
{
    RLogHandler prior;

    rInit(0, 0);

    /*
        assert() in a debug library logs through the error path. Swallow those so a passing run
        is silent, then check that at least one fired: several of the calls below are guarded by
        assert() rather than by a plain test, and silence would mean the guard had gone.
     */
    prior = rSetLogHandler(quietHandler);

    nullClones();
    nullCompares();
    nullSearches();
    nullPredicates();
    nullCopies();
    nullMutators();
    nullFormats();
    nullBuffers();
    nullLists();
    nullHashes();
    nullPaths();
    nullFrees();

    rSetLogHandler(prior);
    ttrue(logged > 0);

    rTerm();
    return 0;
}

/*
    Exceptions - public functions that are NOT null tolerant. Each dereferences its argument
    with no guard, so none is called above.

    sfmtbuf(buf, ...)       - asserts buf and fmt, then formats through them regardless
    sfmtbufv(buf, ...)      - same
    sitosbuf(buf, ...)      - writes through buf with no test
    sjoinbuf(buf, ...)      - copies through buf with no test
    rGrowList(lp, size)     - reads lp->capacity before any test

    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
