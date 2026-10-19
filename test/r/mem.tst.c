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
    char   *cp;
    size_t size;

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
    void   *mp;
    size_t len;

    len = 8 * 1024 * 1024;
    mp = rAlloc(len);
    tnotnull(mp);
    memset(mp, 0, len);
    rFree(mp);
}

static void lotsOfAlloc()
{
    void   *mp;
    void   **links;
    int    depth;
    size_t maxblock, count, i;

    depth = tdepth();
    count = (size_t) ((depth * 5 * 1024) + 1024);
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

    maxblock = (size_t) ((depth * 512 * 1024) + 1024);
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
    void   *blocks[256];
    uchar  *cp;
    int    i, count;
    size_t j, size;

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
        size = (size_t) 1 << ((i + 6) / 100);
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
        size = (size_t) 1 << ((i + 6) / 100);
        for (j = 0; j < size; j++) {
            teqi(cp[j], (i % 0xff));
        }
        rFree(blocks[i]);
    }
}

static void testMemdup()
{
    char   *src, *dup;
    size_t size;

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
        binData[i] = (uchar) i;
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
    char   src[100], dest[100];
    size_t result;
    size_t len;

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
        binSrc[i] = (uchar) i;
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
    for (size_t i = 1; i <= 16; i++) {
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

/*
    rAllocVirt maps pages directly rather than going through the heap. It is what backs fiber
    stacks, so the block must be readable and writable across its whole length.

    The TestMe comparison macros expand their arguments twice, so every call with a side effect
    is bound to a local before it is asserted.
 */
static void virtualAlloc()
{
    char   *ptr;
    size_t size;

    size = 64 * 1024;
    ptr = rAllocVirt(size);
    tnotnull(ptr);

    /*
        Write both ends and the middle, then read them back: a partial mapping would fault.
     */
    ptr[0] = 'a';
    ptr[size / 2] = 'b';
    ptr[size - 1] = 'c';
    teqi(ptr[0], 'a');
    teqi(ptr[size / 2], 'b');
    teqi(ptr[size - 1], 'c');

    memset(ptr, 0, size);
    teqi(ptr[size - 1], 0);

    rFreeVirt(ptr, size);

    /*
        Freeing nothing is safe, which is what lets a failed allocation share the cleanup path.
     */
    rFreeVirt(NULL, size);
}


/*
    The arithmetic guards in the allocator. With a handler installed, rAllocException returns
    to the caller instead of aborting, so a request that cannot be represented is observed as a
    null return and a handler call rather than a dead process.
 */
static int    guardCause;
static size_t guardSize;

static void guardHandler(int cause, size_t size)
{
    guardCause = cause;
    guardSize = size;
}

static void memGuards(void)
{
    char  dst[4];
    void  *p;

    rSetMemHandler(guardHandler);

    //  A size that cannot be aligned without wrapping
    guardCause = 0;
    p = rAllocMem(SIZE_MAX - 4);
    tnull(p);
    teqi(guardCause, R_MEM_FAIL);
    teqz((ssize) guardSize, (ssize) (SIZE_MAX - 4));

    guardCause = 0;
    p = rReallocMem(NULL, SIZE_MAX - 4);
    tnull(p);
    teqi(guardCause, R_MEM_FAIL);

    //  A copy larger than its destination is refused through the same handler
    guardCause = 0;
    teqz((ssize) rMemcpy(dst, sizeof(dst), "0123456789", 10), 0);
    teqi(guardCause, R_ERR_WONT_FIT);
    teqz((ssize) rMemcpy(NULL, sizeof(dst), "01", 2), 0);
    teqz((ssize) rMemcpy(dst, sizeof(dst), NULL, 2), 0);
    teqz((ssize) rMemcpy(dst, sizeof(dst), "01", 0), 0);

    //  A null operand orders below a non-null one, and two nulls are equal
    teqi(rMemcmp(NULL, 0, "a", 1), -1);
    teqi(rMemcmp("a", 1, NULL, 0), 1);
    teqi(rMemcmp(NULL, 0, NULL, 0), 0);

    rSetMemHandler(NULL);
}


/*
    A budgeted allocator: unlimited while the budget is negative, otherwise it fails once the
    budget reaches zero. This is what lets the allocation-failure branches run.
 */
static int    allocBudget = -1;
static int    freeCalls;
static int    failCause;
static size_t failSize;
static int    failCalls;

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
    freeCalls++;
    free(ptr);
}

static void failHandler(int cause, size_t size)
{
    failCause = cause;
    failSize = size;
    failCalls++;
}

static int ownedCalls;

static char *owned(void)
{
    ownedCalls++;
    return sclone("owned");
}

/*
    rFree evaluates its argument once, so freeing the result of a call frees exactly what the
    call allocated. A macro that names its argument twice calls owned() twice and leaks one.
 */
static void freeEvaluatesOnce(void)
{
    char *p;
    int  i;

    rSetAllocator(budgetAlloc, budgetRealloc, budgetFree);
    allocBudget = -1;
    freeCalls = 0;
    ownedCalls = 0;
    for (i = 0; i < 100; i++) {
        rFree(owned());
    }
    teqi(ownedCalls, 100);
    teqi(freeCalls, 100);

    //  Null is still tolerated and rFree is still a single statement
    rFree(NULL);
    p = NULL;
    if (p) rFree(p); else p = sclone("else");
    tmatch(p, "else");
    rFree(p);
    rSetAllocator(NULL, NULL, NULL);
}

static void allocatorInjection(void)
{
    char *p, *q, *r;
    int  i;

    rSetAllocator(budgetAlloc, budgetRealloc, budgetFree);
    rSetMemHandler(failHandler);

    //  Every path is routed: the custom free sees the block the custom alloc handed out
    allocBudget = -1;
    freeCalls = 0;
    p = rAlloc(10);
    tnotnull(p);
    rFree(p);
    teqi(freeCalls, 1);

    //  A failing allocation returns null and reports the aligned size through the handler
    allocBudget = 0;
    failCalls = 0;
    p = rAllocMem(10);
    tnull(p);
    teqi(failCalls, 1);
    teqi(failCause, R_MEM_FAIL);
    teqz((ssize) failSize, 16);

    p = rReallocMem(NULL, 10);
    tnull(p);
    teqi(failCalls, 2);

    p = rAllocZero(32);
    tnull(p);
    teqi(failCalls, 3);

    p = rMemdup("abc", 4);
    tnull(p);
    p = sclone("abc");
    tnull(p);

    //  A failed realloc leaves the original block valid
    allocBudget = 1;
    q = rAlloc(8);
    tnotnull(q);
    allocBudget = 0;
    r = rReallocMem(q, 64);
    tnull(r);
    scopy(q, 8, "still");
    tmatch(q, "still");
    rFree(q);

    //  A zeroed block really is zeroed
    allocBudget = -1;
    p = rAllocZero(32);
    tnotnull(p);
    for (i = 0; i < 32; i++) {
        ttrue(p[i] == 0);
    }
    rFree(p);

    //  A partial allocator is not installed at all
    rSetAllocator(budgetAlloc, NULL, NULL);
    allocBudget = 0;
    p = rAlloc(4);
    tnotnull(p);
    rFree(p);

    allocBudget = -1;
    rSetMemHandler(NULL);
    rSetAllocator(NULL, NULL, NULL);
}

int main(void)
{
    rInit(0, 0);
    virtualAlloc();
    basicAlloc();
    bigAlloc();
    lotsOfAlloc();
    allocIntegrityChecks();
    testMemdup();
    testMemcmp();
    testMemcpy();
    testMemHandlerAndExceptions();
    testEdgeCases();
    memGuards();
    allocatorInjection();
    freeEvaluatesOnce();
    rTerm();
    return 0;
}

/*
    Copyright (c) Michael O'Brien. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
