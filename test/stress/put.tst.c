/*
    put.tst.c - PUT a body that scales with depth, and the first body past the limit

    The body is written through the file handler to the /upload/ directory and the stored size is
    compared with what was sent, so a truncated or padded write fails rather than a mere status
    check passing. The over-limit case asserts both the refusal and that nothing was written: a
    refusal that leaves a partial file behind is the disk-exhaustion path.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "../utils/scenario.h"

/*********************************** Locals ***********************************/

/*
    KB per depth, below web.limits.upload (20MB in test/utils/agent/web.json5). A PUT body is
    bounded by the upload limit, not by limits.body, which governs bodies the handlers buffer.
 */
static const int SIZES_KB[] = { 1, 4, 16, 64, 256, 1024, 4096, 8192, 16384, 20000 };

#define UPLOAD_LIMIT (20 * 1024 * 1024)
#define STORE        "state/site/upload"

/************************************ Code ************************************/
/*
    A printable body of the given size: 49 digits and a newline per line
 */
static char *pattern(size_t size)
{
    char   *buf;
    size_t i;

    buf = rAlloc(size + 1);
    for (i = 0; i < size; i++) {
        buf[i] = ((i % 50) == 49) ? '\n' : (char) ('0' + (i % 10));
    }
    buf[size] = '\0';
    return buf;
}

static int depthIndex(void)
{
    int depth;

    depth = tdepth();
    return depth < 0 ? 0 : (depth > 9 ? 9 : depth);
}

static void testPutScaled(void)
{
    char   url[256], path[256], *body;
    size_t size;
    int    status;

    size = (size_t) SIZES_KB[depthIndex()] * 1024;
    body = pattern(size);
    SFMT(url, "%s/upload/put-scaled.txt", HTTP);
    SFMT(path, "%s/put-scaled.txt", STORE);

    status = request("PUT", url, body, size, NULL);
    tinfo("  PUT %d KB answered %d, stored %lld bytes", (int) (size / 1024), status, (long long) fileSize(path));
    ttrue(status == 201 || status == 204);
    ttrue(fileSize(path) == (int64) size);

    status = request("DELETE", url, NULL, 0, NULL);
    ttrue(status == 200 || status == 204);
    ttrue(fileSize(path) == 0);
    rFree(body);
}

/*
    One byte past web.limits.upload. The limit is enforced on the Content-Length before the body
    is stored, so the response is a 413 or, when the server closes the connection before the
    client has finished writing, a transport error - and in neither case does a file appear.
    Costs a 20 MB write, so it runs from depth 3.
 */
static void testPutOverLimit(void)
{
    char   url[256], path[256], *body;
    size_t size;
    int    status;

    if (tdepth() < 3) {
        tinfo("Skipped: the over-limit PUT runs at depth 3");
        return;
    }
    size = UPLOAD_LIMIT + 1;
    body = pattern(size);
    SFMT(url, "%s/upload/put-over.txt", HTTP);
    SFMT(path, "%s/put-over.txt", STORE);

    status = request("PUT", url, body, size, NULL);
    tinfo("  PUT %d bytes over the %d limit answered %d", (int) size, UPLOAD_LIMIT, status);
    ttrue(status == 413 || status < 0);
    ttrue(fileSize(path) == 0);
    rFree(body);
}

static void fiberMain(void *data)
{
    if (scenarioSetup()) {
        testPutScaled();
        testPutOverLimit();
    }
    rStop();
}

int main(void)
{
    rInit(fiberMain, 0);
    rServiceEvents();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
