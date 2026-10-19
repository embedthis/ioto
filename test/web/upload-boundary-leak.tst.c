/*
    upload-boundary-leak.tst.c - A rejected over-long multipart boundary must not leak

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*
    Use ASan's live-byte accounting when it replaces the platform allocator.
 */
#if defined(__has_feature)
    #if __has_feature(address_sanitizer)
        #define TEST_ASAN 1
    #endif
#endif
#if defined(__SANITIZE_ADDRESS__)
    #define TEST_ASAN 1
#endif

#if TEST_ASAN
    #include <sanitizer/allocator_interface.h>
#elif MACOSX
    #include <malloc/malloc.h>
#elif LINUX && defined(__GLIBC__)
    #include <malloc.h>
#endif

/*********************************** Locals ***********************************/

#define ENDPOINT        "http://127.0.0.1:4853"
#define BOUNDARY_LEN    (32 * 1024)
#define REQUESTS        400

//  Unfixed, the boundaries alone leak REQUESTS * BOUNDARY_LEN (12.5 MB)
#define THRESHOLD       (4 * 1024 * 1024)

#define CONFIG                                                              \
    "{web: {documents: './site', listen: ['" ENDPOINT "'],"                 \
    " limits: {header: '512K', body: '1MB'},"                               \
    " routes: [{match: '/test/', handler: 'action'}, {}]}}"

/************************************ Code ************************************/

/*
    Return the bytes of heap in use, or -1 where it cannot be sampled
 */
static ssize heapInUse(void)
{
#if TEST_ASAN
    return (ssize) __sanitizer_get_current_allocated_bytes();
#elif MACOSX
    malloc_statistics_t stats;

    malloc_zone_statistics(NULL, &stats);
    return (ssize) stats.size_in_use;
#elif LINUX && defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33))
    struct mallinfo2 info;

    info = mallinfo2();
    return (ssize) info.uordblks;
#else
    return -1;
#endif
}

static void sendRequests(char *req, int count)
{
    cchar *segments[1];
    char  buf[1024];
    int   i, status;

    segments[0] = req;
    for (i = 0; i < count; i++) {
        status = rawRequest(ENDPOINT, segments, 1, 0, 1, buf, sizeof(buf));
        if (status != 400) {
            teqi(status, 400, "An over-long boundary must be refused");
            return;
        }
    }
}

static void testRejectedBoundaryIsFreed(void)
{
    WebHost *host;
    ssize   before, after;
    char    *token, *req;

    if (heapInUse() < 0) {
        tskip("Heap in use cannot be sampled on this platform");
        return;
    }
    if ((host = startTestHost(CONFIG)) == 0) {
        return;
    }
    token = rAlloc(BOUNDARY_LEN + 1);
    memset(token, 'B', BOUNDARY_LEN);
    token[BOUNDARY_LEN] = '\0';
    req = sfmt("POST /test/show HTTP/1.1\r\nHost: x\r\n"
               "Content-Type: multipart/form-data; boundary=%s\r\n"
               "Content-Length: 0\r\nConnection: close\r\n\r\n", token);

    //  Warm up so the sample is not measuring first-touch allocation
    sendRequests(req, 20);
    before = heapInUse();
    sendRequests(req, REQUESTS);
    after = heapInUse();

    tinfo("heap in use %lld -> %lld over %d rejected boundaries (%lld bytes)",
          (long long) before, (long long) after, REQUESTS, (long long) (after - before));
    tltll((long long) (after - before), (long long) THRESHOLD, "A rejected boundary must be freed");

    rFree(token);
    rFree(req);
    stopTestHost(host);
}

static void fiberMain(void *arg)
{
    if (setup(NULL, NULL)) {
        testRejectedBoundaryIsFreed();
    }
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
