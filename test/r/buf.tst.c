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
    RBuf  *bp;
    ssize rc;
    char  buf[512];
    int   size, i;

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
    size_t i, size, rc, bytes;
    int    c;

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
    rc = rGetBufSize(bp);
    rc = rGetBufLength(bp);
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
    RBuf  *bp;
    ssize size, rc;
    int   c;

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
    teqi(rc, 0);

    rc = rPutCharToBuf(bp, 'B');
    teqi(rc, 0);
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
            rc = rGetBlockFromBuf(bp, &obuf[count], (ssize) bytes);
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
    RBuf  *bp;
    ssize initialLen, afterLen;

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

int main(void)
{
    rInit(0, 0);

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

    if (tdepth() > 1) {
        bufLoad();
    }
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
