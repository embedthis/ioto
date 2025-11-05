/*
    mem.tst.c - Unit tests for memory ops

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "r.h"

/************************************ Code ************************************/

static void basicAlloc()
{
    char *cp;
    int  size;

    size = 16;
    cp = rAlloc(size);
    tnotnull(cp);
    memset(cp, 0x77, size);
    rFree(cp);

    cp = rAlloc(size);
    tnotnull(cp);
    memset(cp, 0x77, size);
    cp = rRealloc(cp, size * 2);
    tnotnull(cp);
    rFree(cp);

    cp = sclone("Hello World");
    tnotnull(cp);
    tmatch(cp, "Hello World");
    rFree(cp);

    /*
        Test special null tolerant allowances
     */
    cp = sclone(NULL);
    tnotnull(cp);
    teqi(cp[0], '\0');
    rFree(cp);
}

static void bigAlloc()
{
    void  *mp;
    ssize len;

    len = 8 * 1024 * 1024;
    mp = rAlloc(len);
    tnotnull(mp);
    memset(mp, 0, len);
    rFree(mp);
}

static void lotsOfAlloc()
{
    void *mp;
    void **links;
    int  depth, i, maxblock, count;

    depth = tdepth();
    count = (depth * 5 * 1024) + 1024;
    links = rAlloc(count * sizeof(void*));
    tnotnull(links);

    for (i = 0; i < count; i++) {
        mp = rAlloc(64);
        // ttrue(mp != 0);
        links[i] = mp;
    }
    for (i = 0; i < count; i++) {
        rFree(links[i]);
    }
    rFree(links);

    maxblock = (depth * 512 * 1024) + 1024;
    links = rAlloc(maxblock * sizeof(void*));
    for (i = 2; i < maxblock; i *= 2) {
        mp = rAlloc(i);
        // ttrue(mp != 0);
        links[i] = mp;
    }
    for (i = 2; i < maxblock; i *= 2) {
        rFree(links[i]);
    }
    rFree(links);
}

static void allocIntegrityChecks()
{
    void  *blocks[256];
    uchar *cp;
    int   i, j, size, count;

    /*
        Basic integrity test. Allocate blocks of 64 bytes and fill and test each block
     */
    size = 64;
    count = sizeof(blocks) / sizeof(void*);
    for (i = 0; i < count; i++) {
        blocks[i] = rAlloc(size);
        tnotnull(blocks[i]);
        memset(blocks[i], i % 0xff, size);
    }
    for (i = 0; i < count; i++) {
        cp = (uchar*) blocks[i];
        for (j = 0; j < size; j++) {
            teqi(cp[j], (i % 0xff));
        }
    }

    /*
        Now do with bigger blocks and also free some before testing
     */
    count = sizeof(blocks) / sizeof(void*);
    for (i = 1; i < count; i++) {
        size = 1 << ((i + 6) / 100);
        blocks[i] = rAlloc(size);
        tnotnull(blocks[i]);
        memset(blocks[i], i % 0xff, size);
    }
    for (i = 1; i < count; i += 3) {
        blocks[i] = 0;
    }
    for (i = 1; i < count; i++) {
        if (blocks[i] == 0) {
            continue;
        }
        cp = (uchar*) blocks[i];
        size = 1 << ((i + 6) / 100);
        for (j = 0; j < size; j++) {
            teqi(cp[j], (i % 0xff));
        }
        rFree(blocks[i]);
    }
}

static void testMemdup()
{
    char  *src, *dup;
    ssize size;

    // Test basic memdup
    src = "Hello World";
    size = slen(src) + 1;
    dup = rMemdup(src, size);
    tnotnull(dup);
    teqi(scmp(dup, src), 0);
    ttrue(dup != src);  // Different pointers
    rFree(dup);

    // Test NULL input
    dup = rMemdup(NULL, 10);
    tnull(dup);

    // Test zero size
    dup = rMemdup(src, 0);
    tnotnull(dup);
    rFree(dup);

    // Test binary data
    uchar binData[256];
    for (int i = 0; i < 256; i++) {
        binData[i] = i;
    }
    dup = rMemdup(binData, 256);
    tnotnull(dup);
    teqi(memcmp(dup, binData, 256), 0);
    rFree(dup);

    // Test large memdup
    size = 64 * 1024;
    src = rAlloc(size);
    memset(src, 0xAA, size);
    dup = rMemdup(src, size);
    tnotnull(dup);
    teqi(memcmp(dup, src, size), 0);
    rFree(src);
    rFree(dup);
}

static void testMemcmp()
{
    char *s1, *s2;

    // Test equal strings, equal length
    s1 = "Hello";
    s2 = "Hello";
    teqi(rMemcmp(s1, 5, s2, 5), 0);

    // Test different strings, equal length
    s1 = "Hello";
    s2 = "World";
    ttrue(rMemcmp(s1, 5, s2, 5) < 0);

    // Test equal content, different lengths
    s1 = "Hello";
    s2 = "Hello World";
    teqi(rMemcmp(s1, 5, s2, 5), 0);

    // Test s1 shorter than s2
    s1 = "Hell";
    s2 = "Hello";
    ttrue(rMemcmp(s1, 4, s2, 5) < 0);

    // Test s1 longer than s2
    s1 = "Hello";
    s2 = "Hell";
    ttrue(rMemcmp(s1, 5, s2, 4) > 0);

    // Test zero lengths
    teqi(rMemcmp(s1, 0, s2, 0), 0);

    // Test one zero length
    ttrue(rMemcmp(s1, 0, s2, 4) < 0);
    ttrue(rMemcmp(s1, 4, s2, 0) > 0);

    // Test binary data
    uchar b1[] = { 0x01, 0x02, 0x03, 0x04 };
    uchar b2[] = { 0x01, 0x02, 0x03, 0x05 };
    ttrue(rMemcmp(b1, 4, b2, 4) < 0);
    teqi(rMemcmp(b1, 3, b2, 3), 0);
}

static void testMemcpy()
{
    char  src[100], dest[100];
    ssize result;
    ssize len;

    // Test basic copy
    scopy(src, sizeof(src), "Hello World");
    len = slen(src);
    result = rMemcpy(dest, sizeof(dest), src, len + 1);
    teqz(result, len + 1);
    teqi(scmp(dest, src), 0);

    // Test zero bytes
    result = rMemcpy(dest, sizeof(dest), src, 0);
    teqz(result, 0);

    // Test overlapping regions (insitu copy)
    scopy(src, sizeof(src), "Hello World Test");
    result = rMemcpy(src + 6, 50, src, 5);  // Copy "Hello" to position 6
    teqz(result, 5);
    teqi(memcmp(src + 6, "Hello", 5), 0);

    // Test boundary condition - exact fit
    scopy(src, sizeof(src), "Test");
    result = rMemcpy(dest, 5, src, 5);
    teqz(result, 5);

    // Test buffer overflow protection - should trigger exception
    // Note: This would normally call rAllocException, but we can't easily test that here
    // without setting up a custom handler

    // Test binary data
    uchar binSrc[50], binDest[50];
    for (int i = 0; i < 50; i++) {
        binSrc[i] = i;
    }
    result = rMemcpy(binDest, sizeof(binDest), binSrc, 50);
    teqz(result, 50);
    teqi(memcmp(binDest, binSrc, 50), 0);
}

static int    memHandlerCalled = 0;
static int    lastCause = 0;
static size_t lastSize = 0;

static void testMemHandler(int cause, size_t size)
{
    memHandlerCalled++;
    lastCause = cause;
    lastSize = size;
}

static void testMemHandlerAndExceptions()
{
    // Reset test state
    memHandlerCalled = 0;
    lastCause = 0;
    lastSize = 0;

    // Install test handler
    rSetMemHandler(testMemHandler);

    // Test exception triggering
    rAllocException(R_MEM_FAIL, 1024);
    teqi(memHandlerCalled, 1);
    teqi(lastCause, R_MEM_FAIL);
    teqz(lastSize, 1024);

    // Test another exception
    rAllocException(R_ERR_WONT_FIT, 2048);
    teqi(memHandlerCalled, 2);
    teqi(lastCause, R_ERR_WONT_FIT);
    teqz(lastSize, 2048);

    // Reset handler to default (NULL will use default behavior)
    rSetMemHandler(NULL);
}

static void testEdgeCases()
{
    void *ptr;

    // Test zero-size allocation (should allocate 1 byte minimum)
    ptr = rAllocMem(0);
    tnotnull(ptr);
    rFreeMem(ptr);

    // Test rFreeMem with NULL (should not crash)
    rFreeMem(NULL);

    // Test rMemcpy with NULL pointers
    teqz(rMemcpy(NULL, 10, "test", 4), 0);

    char dest[10];
    teqz(rMemcpy(dest, 10, NULL, 4), 0);

    // Test very small allocations
    for (int i = 1; i <= 16; i++) {
        ptr = rAllocMem(i);
        tnotnull(ptr);
        rFreeMem(ptr);
    }

    // Test realloc with NULL (should behave like malloc)
    ptr = rReallocMem(NULL, 100);
    tnotnull(ptr);
    rFreeMem(ptr);

    // Test realloc to zero size
    ptr = rAllocMem(100);
    tnotnull(ptr);
    ptr = rReallocMem(ptr, 0);
    // Note: realloc behavior with size 0 is implementation-defined
    // Some implementations free the memory, others allocate minimum size
    if (ptr) {
        rFreeMem(ptr);
    }
}

int main(void)
{
    rInit(0, 0);
    basicAlloc();
    bigAlloc();
    lotsOfAlloc();
    allocIntegrityChecks();
    testMemdup();
    testMemcmp();
    testMemcpy();
    testMemHandlerAndExceptions();
    testEdgeCases();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
