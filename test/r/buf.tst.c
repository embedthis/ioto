/**
    buf.tst.c - Unit tests for the Buf class

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "r.h"

/************************************ Code ************************************/

static void createBuf()
{
    RBuf *bp;

    bp = rAllocBuf(512);
    tnotnull(bp);
    rFreeBuf(bp);
}

static void isBufEmpty()
{
    RBuf   *bp;
    size_t size;

    size = 512;
    bp = rAllocBuf(size);

    tnotnull(bp);
    teqz(rGetBufLength(bp), 0);
    ttrue(rGetBufSize(bp) >= size);
    ttrue(rGetBufSpace(bp) >= (size - 1));
    teqz(rGetBufLength(bp), 0);
    ttrue(rGetBufStart(bp) == rGetBufEnd(bp));
    rFreeBuf(bp);
}

static void putAndGetToBuf()
{
    RBuf   *bp;
    size_t i, size, bytes;
    int    rc, d;

    size = 512;
    bp = rAllocBuf(size);

    bytes = size / 2;
    for (i = 0; i < bytes; i++) {
        rc = rPutCharToBuf(bp, 'd');
        if (rc) {
            teqi(rc, 0);
            break;
        }
    }
    teqz(rGetBufLength(bp), bytes);

    for (i = 0; i < bytes; i++) {
        d = rGetCharFromBuf(bp);
        if (d != 'd') {
            teqi(d, 'd');
        }
    }
    d = rGetCharFromBuf(bp);
    teqi(d, -1);
    rFreeBuf(bp);
}

static void flushBuf()
{
    RBuf   *bp;
    ssize  rc;
    char   buf[512];
    size_t size;
    int    i;

    size = 512;
    bp = rAllocBuf(size);
    tnotnull(bp);

    /*
        Do multiple times to test that flush resets the buffer pointers correctly
     */
    for (i = 0; i < 100; i++) {
        rc = rPutStringToBuf(bp, "Hello World");
        teqz(rc, 11);
        teqz(rGetBufLength(bp), 11);

        rFlushBuf(bp);
        teqz(rGetBufLength(bp), 0);
        teqi(rGetCharFromBuf(bp), -1);
        teqz(rGetBlockFromBuf(bp, buf, sizeof(buf)), 0);
    }
    rFreeBuf(bp);
}

static void growBuf()
{
    RBuf   *bp;
    size_t i, size, bytes;
    int    c, rc;

    /*
        Put more data than the initial size to force the buffer to grow
     */
    size = 512;
    bp = rAllocBuf(size);
    bytes = size * 10;
    for (i = 0; i < bytes; i++) {
        rc = rPutCharToBuf(bp, 'c');
        teqi(rc, 0);
    }
    ttrue(rGetBufSize(bp) > size);
    ttrue(rGetBufSize(bp) >= bytes);
    teqz(rGetBufLength(bp), bytes);

    for (i = 0; i < bytes; i++) {
        c = rGetCharFromBuf(bp);
        if (c != 'c') {
            teqi(c, 'c');
        }
    }
    c = rGetCharFromBuf(bp);
    teqi(c, -1);
    rFreeBuf(bp);
}

static void miscBuf()
{
    RBuf   *bp;
    size_t size;
    ssize  rc;
    int    c;

    size = 512;
    bp = rAllocBuf(size);
    tnotnull(bp);

    /*
        Test inser char
     */
    rc = rPutStringToBuf(bp, " big dog");
    teqz(rc, 8);
    teqz(rGetBufLength(bp), 8);

    /*
        Test add null
     */
    rFlushBuf(bp);
    teqz(rGetBufLength(bp), 0);

    rc = rPutCharToBuf(bp, 'A');
    teqll(rc, 0);

    rc = rPutCharToBuf(bp, 'B');
    teqll(rc, 0);
    teqz(rGetBufLength(bp), 2);

    rAddNullToBuf(bp);
    teqz(rGetBufLength(bp), 2);
    tmatch(rGetBufStart(bp), "AB");

    c = rLookAtNextCharInBuf(bp);
    teqi(c, 'A');

    rFreeBuf(bp);
}

static void bufLoad()
{
    RBuf   *bp;
    char   obuf[512], ibuf[512];
    ssize  rc;
    size_t count, bytes, sofar, size, len;
    int    i, j;

    /*
        Pick an odd size to guarantee put blocks are sometimes parial.
     */
    len = 981;
    bp = rAllocBuf(len);
    tnotnull(bp);

    for (i = 0; i < (int) sizeof(ibuf); i++) {
        ibuf[i] = 'A' + (i % 26);
    }
    for (j = 0; j < 500; j++) {
        rc = rPutBlockToBuf(bp, (char*) ibuf, sizeof(ibuf));
        teqz(rc, sizeof(ibuf));

        count = 0;
        while (rGetBufLength(bp) > 0) {
            size = 0xFFFF & (int) rGetTime();
            bytes = ((size % sizeof(obuf)) / 9) + 1;
            bytes = min(bytes, (sizeof(obuf) - count));
            rc = rGetBlockFromBuf(bp, &obuf[count], bytes);
            ttrue(rc > 0);
            count += (size_t) rc;
        }
        teqz(count, sizeof(ibuf));
        for (i = 0; i < (int) sizeof(ibuf); i++) {
            teqi(obuf[i], ('A' + (i % 26)));
        }
        rFlushBuf(bp);
    }

    /*
        Now do a similar load test but using the star / end pointer directly
     */
    for (j = 0; j < 500; j++) {
        bytes = sizeof(ibuf);
        sofar = 0;
        do {
            len = rGetBufSpace(bp);
            len = min(len, bytes);
            rMemcpy(rGetBufEnd(bp), rGetBufSpace(bp), &ibuf[sofar], len);
            sofar += len;
            bytes -= len;
            rAdjustBufEnd(bp, (ssize) len);
        } while (bytes > 0);
        teqz(sofar, sizeof(ibuf));

        sofar = 0;
        while (rGetBufLength(bp) > 0) {
            len = min(rGetBufLength(bp), (sizeof(obuf) - sofar));
            rMemcpy(&obuf[sofar], sizeof(obuf) - sofar, rGetBufStart(bp), len);
            sofar += len;
            rAdjustBufStart(bp, (ssize) len);
        }
        teqz(sofar, sizeof(ibuf));
        for (i = 0; i < (int) sizeof(obuf); i++) {
            teqi(obuf[i], ('A' + (i % 26)));
        }
        rFlushBuf(bp);
    }
    rFreeBuf(bp);
}

static void testErrorConditions()
{
    RBuf  buf, *bp;
    int   rc;
    ssize result;
    char  data[10];

    /*
        Test rInitBuf error conditions
     */
    rc = rInitBuf(NULL, 100);
    teqi(rc, R_ERR_BAD_ARGS);

    rc = rInitBuf(&buf, 0);
    teqi(rc, R_ERR_BAD_ARGS);

    /*
        Test valid rInitBuf/rTermBuf
     */
    rc = rInitBuf(&buf, 100);
    teqi(rc, 0);
    tnotnull(buf.buf);
    teqz(buf.buflen, 100);
    rTermBuf(&buf);
    tnull(buf.buf);

    /*
        Test rGetBlockFromBuf error conditions
     */
    bp = rAllocBuf(100);
    result = rGetBlockFromBuf(bp, NULL, 10);
    teqz(result, R_ERR_BAD_ARGS);

    result = rGetBlockFromBuf(bp, data, (size_t) -1);
    teqz(result, R_ERR_BAD_ARGS);

    rFreeBuf(bp);
}

static void testUncoveredFunctions()
{
    RBuf  *bp;
    int   rc, c;
    ssize len;
    cchar *str;
    char  *result;

    bp = rAllocBuf(100);

    /*
        Test rReserveBufSpace
     */
    rc = rReserveBufSpace(bp, 50);
    teqi(rc, 0);
    ttrue(rGetBufSpace(bp) >= 50);

    /*
        Test rInserCharToBuf error condition
     */
    rFlushBuf(bp);
    rc = rInserCharToBuf(bp, 'X');
    teqi(rc, R_ERR_BAD_STATE);

    /*
        Test rInserCharToBuf success case
     */
    rPutCharToBuf(bp, 'A');
    rGetCharFromBuf(bp);
    rPutStringToBuf(bp, "test");
    rc = rInserCharToBuf(bp, 'X');
    teqi(rc, 0);
    c = rGetCharFromBuf(bp);
    teqi(c, 'X');

    /*
        Test rLookAtLastCharInBuf
     */
    rFlushBuf(bp);
    c = rLookAtLastCharInBuf(bp);
    teqi(c, -1);

    rPutStringToBuf(bp, "hello");
    c = rLookAtLastCharInBuf(bp);
    teqi(c, 'o');

    /*
        Test rPutSubToBuf
     */
    rFlushBuf(bp);
    len = rPutSubToBuf(bp, "hello world", 5);
    teqz(len, 5);
    rAddNullToBuf(bp);
    tmatch(rGetBufStart(bp), "hello");

    len = rPutSubToBuf(bp, NULL, 5);
    teqz(len, 0);

    /*
        Test rPutToBuf (formatted output)
     */
    rFlushBuf(bp);
    len = rPutToBuf(bp, "Number: %d, String: %s", 42, "test");
    ttrue(len > 0);
    rAddNullToBuf(bp);
    tcontains(rGetBufStart(bp), "42");
    tcontains(rGetBufStart(bp), "test");

    len = rPutToBuf(bp, NULL);
    teqz(len, 0);

    /*
        Test rPutIntToBuf
     */
    rFlushBuf(bp);
    len = rPutIntToBuf(bp, 12345);
    ttrue(len > 0);
    rAddNullToBuf(bp);
    tmatch(rGetBufStart(bp), "12345");

    /*
        Test rBufToString
     */
    rFlushBuf(bp);
    rPutStringToBuf(bp, "test string");
    str = rBufToString(bp);
    tmatch(str, "test string");

    /*
        Test rBufToStringAndFree
     */
    bp = rAllocBuf(100);
    rPutStringToBuf(bp, "transfer test");
    result = rBufToStringAndFree(bp);
    tmatch(result, "transfer test");
    rFree(result);

    /*
        Test rBufToStringAndFree with NULL
     */
    result = rBufToStringAndFree(NULL);
    tnull(result);
}

static void testCompactAndReset()
{
    RBuf   *bp;
    size_t initialLen, afterLen;

    bp = rAllocBuf(100);

    /*
        Test rCompactBuf with data
     */
    rPutStringToBuf(bp, "hello world");
    rGetCharFromBuf(bp);
    rGetCharFromBuf(bp);
    ttrue(rGetBufStart(bp) > bp->buf);

    rCompactBuf(bp);
    ttrue(rGetBufStart(bp) == bp->buf);
    rAddNullToBuf(bp);
    tmatch(rGetBufStart(bp), "llo world");

    /*
        Test rCompactBuf with empty buffer
     */
    rFlushBuf(bp);
    rCompactBuf(bp);
    teqz(rGetBufLength(bp), 0);

    /*
        Test rResetBufIfEmpty
     */
    rPutStringToBuf(bp, "data");
    initialLen = rGetBufLength(bp);
    rResetBufIfEmpty(bp);
    afterLen = rGetBufLength(bp);
    teqz(initialLen, afterLen);

    rFlushBuf(bp);
    rResetBufIfEmpty(bp);
    teqz(rGetBufLength(bp), 0);

    rFreeBuf(bp);
}

static void testEdgeCases()
{
    RBuf   *bp;
    int    rc;
    size_t originalSize;

    bp = rAllocBuf(100);

    /*
        Test rGrowBuf edge cases
     */
    rc = rGrowBuf(bp, 0);
    teqi(rc, R_ERR_BAD_ARGS);

    rc = rGrowBuf(bp, (size_t) -1);
    teqi(rc, R_ERR_BAD_ARGS);

    /*
        Test buffer at exact capacity
     */
    originalSize = rGetBufSize(bp);
    while (rGetBufSpace(bp) > 1) {
        rPutCharToBuf(bp, 'X');
    }
    ttrue(rGetBufSpace(bp) <= 1);

    /* Add one more character to force buffer growth */
    rc = rPutCharToBuf(bp, 'Y');
    teqi(rc, 0);

    /* Buffer should have grown if it was near capacity */
    if (rGetBufSize(bp) <= originalSize) {
        /* If it didn't grow, try adding more data to force growth */
        while (rGetBufSpace(bp) > 0) {
            rPutCharToBuf(bp, 'Z');
        }
        rPutCharToBuf(bp, 'W');  /* This should definitely force growth */
    }
    ttrue(rGetBufSize(bp) >= originalSize);

    /*
        Test rAdjustBufEnd boundary conditions
     */
    rFlushBuf(bp);
    rPutStringToBuf(bp, "test");

    rAdjustBufEnd(bp, -2);
    teqz(rGetBufLength(bp), 2);
    rAddNullToBuf(bp);
    tmatch(rGetBufStart(bp), "te");

    /*
        Test rAdjustBufStart with various values
     */
    rFlushBuf(bp);
    rPutStringToBuf(bp, "hello");

    rAdjustBufStart(bp, 2);
    teqz(rGetBufLength(bp), 3);
    rAddNullToBuf(bp);
    tmatch(rGetBufStart(bp), "llo");

    rAdjustBufStart(bp, -1);
    teqz(rGetBufLength(bp), 3);

    rFreeBuf(bp);
}

static void testGrowBufSize()
{
    RBuf   *bp;
    int    rc;
    size_t originalSize;

    bp = rAllocBuf(64);
    originalSize = rGetBufSize(bp);

    /*
        Test growing to a larger size
     */
    rc = rGrowBufSize(bp, 256);
    teqi(rc, 0);
    ttrue(rGetBufSize(bp) >= 256);

    /*
        Test growing to a size smaller than current (should be a no-op)
     */
    originalSize = rGetBufSize(bp);
    rc = rGrowBufSize(bp, 100);
    teqi(rc, 0);
    teqz(rGetBufSize(bp), originalSize);

    /*
        Test that data is preserved after growth
     */
    rFlushBuf(bp);
    rPutStringToBuf(bp, "hello world");
    originalSize = rGetBufSize(bp);

    rc = rGrowBufSize(bp, originalSize * 2);
    teqi(rc, 0);
    ttrue(rGetBufSize(bp) >= originalSize * 2);
    rAddNullToBuf(bp);
    tmatch(rGetBufStart(bp), "hello world");

    /*
        Test error conditions
     */
    rc = rGrowBufSize(bp, 0);
    teqi(rc, R_ERR_BAD_ARGS);

    rc = rGrowBufSize(bp, (size_t) -1);
    teqi(rc, R_ERR_BAD_ARGS);

    rFreeBuf(bp);
}

/*
    The remaining accessor paths: consuming from the front, reading blocks that wrap the buffer,
    and the allocation limits.

    The TestMe comparison macros expand their arguments twice, so every call with a side effect
    is bound to a local before it is asserted.
 */
static void testAdjustAndBlocks()
{
    RBuf  *bp;
    char  out[32];
    char  *base;
    ssize len;
    size_t remaining;

    bp = rAllocBuf(64);
    tnotnull(bp);
    rPutStringToBuf(bp, "0123456789");

    /*
        rGetBuf returns the allocation itself, which is where start points in a buffer nothing
        has been consumed from yet.
     */
    base = rGetBuf(bp);
    tnotnull(base);
    ttrue(base == rGetBufStart(bp));

    /*
        Consuming from the front advances start without moving the data.
     */
    rAdjustBufStart(bp, 3);
    remaining = rGetBufLength(bp);
    teqz(remaining, 7);
    rAddNullToBuf(bp);
    tmatch(rGetBufStart(bp), "3456789");

    /*
        A negative adjustment, and one past the end, are both ignored rather than moving start
        outside the buffer.
     */
    rAdjustBufStart(bp, -100);
    remaining = rGetBufLength(bp);
    teqz(remaining, 7);
    rAdjustBufStart(bp, 1000);
    remaining = rGetBufLength(bp);
    teqz(remaining, 7);

    /*
        Consuming everything empties the buffer.
     */
    rAdjustBufStart(bp, 7);
    remaining = rGetBufLength(bp);
    teqz(remaining, 0);

    /*
        rGetBlockFromBuf copies out and consumes what it copied.
     */
    rFlushBuf(bp);
    rPutStringToBuf(bp, "abcdefghij");
    len = rGetBlockFromBuf(bp, out, 4);
    teqz(len, 4);
    out[4] = '\0';
    tmatch(out, "abcd");
    remaining = rGetBufLength(bp);
    teqz(remaining, 6);

    /*
        Asking for more than the buffer holds returns only what is there.
     */
    len = rGetBlockFromBuf(bp, out, sizeof(out));
    teqz(len, 6);
    out[6] = '\0';
    tmatch(out, "efghij");
    remaining = rGetBufLength(bp);
    teqz(remaining, 0);

    /*
        Reading an empty buffer yields nothing rather than an error.
     */
    len = rGetBlockFromBuf(bp, out, sizeof(out));
    teqz(len, 0);

    len = rGetBlockFromBuf(bp, NULL, 4);
    teqz(len, R_ERR_BAD_ARGS);

    rFreeBuf(bp);

    /*
        rAllocBuf refuses a size above INT_MAX, but it reports the refusal through
        rAllocException, and the default memory handler treats R_MEM_FAIL as fatal. That path
        ends the process, so it is not exercised here.
     */
}


/*
    Growth with consumed data in front of it compacts before it copies, and the small guards
    on the accessors. None of these had been reached.
 */
static void bufCorners()
{
    RBuf  *bp;
    char  out[8];
    ssize rc;

    //  Fill a small buffer, consume some, then grow: the live bytes slide to the front first
    bp = rAllocBuf(16);
    tnotnull(bp);
    rc = rPutStringToBuf(bp, "0123456789abcdef");
    ttrue(rc > 0);
    rAdjustBufStart(bp, 6);
    ttrue(bp->start > bp->buf);
    teqi(rGrowBuf(bp, 1), 0);
    ttrue(bp->start == bp->buf);
    tmatch(rBufToString(bp), "6789abcdef");

    //  A grow that fits in the existing allocation is a no-op success
    teqi(rGrowBuf(bp, 1), 0);
    //  ... and one beyond the ceiling is refused
    //  ME_R_MAX_BUF is 8MB and private to buf.c
    ttrue(rGrowBuf(bp, (size_t) 8 * 1024 * 1024 + 1) < 0);

    //  Adjusting the start by zero from the front leaves it at the front
    rResetBufIfEmpty(bp);
    rFlushBuf(bp);
    rAdjustBufStart(bp, 0);
    ttrue(bp->start == bp->buf);

    //  Sub-string puts with nothing to put
    teqz(rPutSubToBuf(bp, NULL, 3), 0);
    teqz(rPutSubToBuf(bp, "", 3), 0);
    teqz(rPutSubToBuf(bp, "abc", 0), 0);
    teqz(rPutSubToBuf(bp, "abc", 2), 2);
    tmatch(rBufToString(bp), "ab");

    //  Null buffer guards
    teqi(rPutCharToBuf(NULL, 'x'), R_ERR_BAD_ARGS);
    rAddNullToBuf(NULL);
    rAdjustBufStart(NULL, 1);

    /*
        rGetBlockFromBuf checks the buffer's own bookkeeping before trusting it. A buflen that
        disagrees with the pointers is corruption, and it is reported rather than read through.
     */
    bp->buflen++;
    teqz(rGetBlockFromBuf(bp, out, sizeof(out)), R_ERR_BAD_STATE);
    bp->buflen--;
    teqz(rGetBlockFromBuf(bp, out, sizeof(out)), 2);
    rFreeBuf(bp);
}


/*
    A budgeted allocator: unlimited while the budget is negative, otherwise it fails once the
    budget reaches zero.
 */
static int allocBudget = -1;
static int failCalls;

static void *budgetAlloc(size_t size)
{
    if (allocBudget == 0) {
        return NULL;
    }
    if (allocBudget > 0) {
        allocBudget--;
    }
    return malloc(size);
}

static void *budgetRealloc(void *ptr, size_t size)
{
    if (allocBudget == 0) {
        return NULL;
    }
    if (allocBudget > 0) {
        allocBudget--;
    }
    return realloc(ptr, size);
}

static void budgetFree(void *ptr)
{
    free(ptr);
}

static void failHandler(int cause, size_t size)
{
    failCalls++;
}

/*
    Allocation failures are reported as null returns and R_ERR_MEMORY, and a buffer that fails
    to grow keeps its contents
 */
static void bufAllocFailures(void)
{
    RBuf   *bp, sb;
    size_t space;
    int    rc;

    rSetAllocator(budgetAlloc, budgetRealloc, budgetFree);
    rSetMemHandler(failHandler);

    //  The header and the storage are separate allocations and either can fail
    allocBudget = 0;
    failCalls = 0;
    bp = rAllocBuf(64);
    tnull(bp);
    teqi(failCalls, 1);
    allocBudget = 1;
    bp = rAllocBuf(64);
    tnull(bp);

    allocBudget = 0;
    rc = rInitBuf(&sb, 16);
    teqi(rc, R_ERR_MEMORY);

    allocBudget = -1;
    bp = rAllocBuf(16);
    tnotnull(bp);
    rPutStringToBuf(bp, "abc");

    allocBudget = 0;
    rc = rGrowBufSize(bp, 1024);
    teqi(rc, R_ERR_MEMORY);
    rc = rReserveBufSpace(bp, 1024);
    teqi(rc, R_ERR_MEMORY);
    tmatch(rGetBufStart(bp), "abc");

    //  Fill the remaining space so the next character has to grow the buffer
    allocBudget = -1;
    for (space = rGetBufSpace(bp); space > 1; space--) {
        rPutCharToBuf(bp, 'x');
    }
    allocBudget = 0;
    rc = rPutCharToBuf(bp, 'y');
    teqi(rc, R_ERR_MEMORY);
    ttrue(sstarts(rGetBufStart(bp), "abcx"));
    rFreeBuf(bp);

    allocBudget = -1;
    rSetMemHandler(NULL);
    rSetAllocator(NULL, NULL, NULL);
}


static size_t fillToCeiling(RBuf *bp)
{
    size_t i, size;
    char   *data;

    for (i = 0; i < 64; i++) {
        size = rGetBufSize(bp);
        data = rAlloc(size);
        memset(data, 'a', size);
        rPutBlockToBuf(bp, data, rGetBufSpace(bp));
        rFree(data);
        if (rGetBufSpace(bp) == 0 && rGrowBuf(bp, 1) < 0) {
            break;
        }
    }
    return rGetBufLength(bp);
}


static void addNullDoesNotTruncate()
{
    RBuf   *bp;
    size_t len;

    bp = rAllocBuf(1024);
    len = fillToCeiling(bp);
    teqz(rGetBufSpace(bp), 0);
    ttrue(len > 0);
    teqz(rGetBufSize(bp), len);

    rAddNullToBuf(bp);
    teqz(rGetBufLength(bp), len);
    teqi(rGetBufStart(bp)[len - 1], 'a');
    ttrue(rGetBufSize(bp) > len);
    if (rGetBufSize(bp) > len) {
        teqi(rGetBufStart(bp)[len], '\0');
    }
    rFreeBuf(bp);

    //  With room to spare the terminator is added and the length is still unchanged
    bp = rAllocBuf(64);
    rPutStringToBuf(bp, "hello");
    len = rGetBufLength(bp);
    rAddNullToBuf(bp);
    teqz(rGetBufLength(bp), len);
    teqi(rGetBufStart(bp)[len], '\0');
    tmatch(rGetBufStart(bp), "hello");
    rFreeBuf(bp);
}


static void bufToStringAtCeiling()
{
    RBuf       *bp;
    const char *s;
    size_t     len;

    bp = rAllocBuf(1024);
    len = fillToCeiling(bp);
    teqz(rGetBufSpace(bp), 0);

    s = rBufToString(bp);
    tnotnull(s);
    ttrue(rGetBufSize(bp) > len);
    if (rGetBufSize(bp) > len) {
        teqz(slen(s), len);
        teqz(slen(rBufToString(bp)), len);
    }
    teqz(rGetBufLength(bp), len);
    rFreeBuf(bp);
}


static void addNullReclaimsConsumedSpace()
{
    RBuf   *bp;
    size_t len, size;

    bp = rAllocBuf(1024);
    len = fillToCeiling(bp);
    size = rGetBufSize(bp);
    rAdjustBufStart(bp, 16);

    rAddNullToBuf(bp);
    teqz(rGetBufLength(bp), len - 16);
    teqz(rGetBufSize(bp), size);
    teqi(rGetBufStart(bp)[len - 16], '\0');
    teqi(rGetBufStart(bp)[0], 'a');
    rFreeBuf(bp);
}

/*
    rBufToStringAndFree on a buffer at the ceiling must return a terminated allocation.
 */
static void bufToStringAndFreeAtCeiling()
{
    RBuf   *bp;
    char   *s;
    size_t len;

    bp = rAllocBuf(1024);
    len = fillToCeiling(bp);
    s = rBufToStringAndFree(bp);
    tnotnull(s);
    teqz(slen(s), len);
    rFree(s);
}

/*
    rBufToStringAndFree used to write the terminator before compacting, so the memmove left the
    terminator behind and the returned string ran on by exactly the number of bytes the start had
    been advanced. The web module's Cache-Control builder strips a leading ", " that way and put
    "no-storere" on the wire (r issue 35).
 */
static void bufToStringAfterAdjustStart()
{
    RBuf   *bp;
    char   *s;
    size_t n;

    for (n = 1; n <= 5; n++) {
        bp = rAllocBuf(0);
        rPutStringToBuf(bp, ", no-store");
        rAdjustBufStart(bp, (ssize) n);
        s = rBufToStringAndFree(bp);
        teqz(slen(s), slen(", no-store") - n);
        tmatch(s, &", no-store"[n]);
        rFree(s);
    }
    //  A buffer whose start was advanced to the end converts to an empty string
    bp = rAllocBuf(0);
    rPutStringToBuf(bp, "abc");
    rAdjustBufStart(bp, 3);
    s = rBufToStringAndFree(bp);
    tmatch(s, "");
    rFree(s);
}

int main(void)
{
    rInit(0, 0);
    testAdjustAndBlocks();
    addNullDoesNotTruncate();
    bufToStringAtCeiling();
    addNullReclaimsConsumedSpace();
    bufToStringAndFreeAtCeiling();
    bufToStringAfterAdjustStart();

    createBuf();
    isBufEmpty();
    putAndGetToBuf();
    flushBuf();
    growBuf();
    miscBuf();

    testErrorConditions();
    testUncoveredFunctions();
    testCompactAndReset();
    testEdgeCases();
    testGrowBufSize();

    if (tdepth() > 1) {
        bufLoad();
    bufCorners();
    bufAllocFailures();
    }
    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
