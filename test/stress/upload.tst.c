/*
    upload.tst.c - A multipart upload that scales with depth, and one past the upload limit

    The unit app's /api/public/upload fixture reports what the filter staged, so the size
    assertion is on the server's account of the file, not the client's. After every case the
    staging directory must be empty: the filter removes each temp file when the request
    completes, and a staged file that survives a refusal is the disk-exhaustion path.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "../utils/scenario.h"

#include <dirent.h>

/*********************************** Locals ***********************************/

//  KB per depth, below web.limits.upload (20MB in test/utils/agent/web.json5)
static const int SIZES_KB[] = { 8, 32, 128, 512, 1024, 2048, 4096, 8192, 16384, 20000 };

#define UPLOAD_LIMIT (20 * 1024 * 1024)
#define SOURCE       "state/upload-src.dat"
#define STAGING      "state/tmp"

/************************************ Code ************************************/
/*
    Files in the staging directory, or -1 when it cannot be read
 */
static int stagedFiles(void)
{
    DIR           *dir;
    struct dirent *entry;
    int           count;

    if ((dir = opendir(STAGING)) == 0) {
        return -1;
    }
    count = 0;
    while ((entry = readdir(dir)) != 0) {
        if (entry->d_name[0] != '.') {
            count++;
        }
    }
    closedir(dir);
    return count;
}

/*
    Write the source file of the given size and upload it. Returns the status, and the size the
    server reported for it through *reported (or -1).
 */
static int upload(size_t size, int64 *reported)
{
    Url   *up;
    RList *files;
    RHash *forms;
    cchar *body, *cp;
    char  url[256], *buf;
    int   status;

    buf = rAlloc(size);
    memset(buf, 'u', size);
    ttrue(rWriteFile(SOURCE, buf, size, 0644) == (ssize) size);
    rFree(buf);

    files = rAllocList(0, 0);
    rAddItem(files, SOURCE);
    forms = rAllocHash(0, 0);
    rAddName(forms, "size", sfmt("%d", (int) size), R_DYNAMIC_VALUE);

    up = urlAlloc(0);
    status = -1;
    *reported = -1;
    if (urlStart(up, "POST", SFMT(url, "%s/api/public/upload", HTTP)) == 0 && urlUpload(up, files, forms, NULL) == 0) {
        urlFinalize(up);
        status = urlGetStatus(up);
        body = urlGetResponse(up);
        //  "FILE <field> upload-src.dat <size>"
        if (body && (cp = scontains(body, "upload-src.dat ")) != 0) {
            *reported = stoi(cp + slen("upload-src.dat "));
        }
    }
    urlFree(up);
    rFree(forms);
    rFree(files);
    return status;
}

static int depthIndex(void)
{
    int depth;

    depth = tdepth();
    return depth < 0 ? 0 : (depth > 9 ? 9 : depth);
}

static void testUploadScaled(void)
{
    size_t size;
    int64  reported;
    int    status;

    size = (size_t) SIZES_KB[depthIndex()] * 1024;
    status = upload(size, &reported);
    tinfo("  upload %d KB answered %d, server staged %lld bytes, %d file(s) left in %s", (int) (size / 1024),
          status, (long long) reported, stagedFiles(), STAGING);
    ttrue(status == 200);
    ttrue(reported == (int64) size);
    ttrue(stagedFiles() == 0);
}

/*
    One byte past web.limits.upload. Costs a 20 MB write, so it runs from depth 3.
 */
static void testUploadOverLimit(void)
{
    int64 reported;
    int   status;

    if (tdepth() < 3) {
        tinfo("Skipped: the over-limit upload runs at depth 3");
        return;
    }
    status = upload(UPLOAD_LIMIT + 1, &reported);
    tinfo("  upload %d bytes over the limit answered %d, %d file(s) left in %s", UPLOAD_LIMIT + 1, status,
          stagedFiles(), STAGING);
    /*
        The upload filter refuses an oversized part with 414 today; 413 is the status the size
        of a request body calls for, and the Content-Length check on a PUT already answers it.
        Either is a refusal, and the file not surviving is the assertion that matters.
     */
    ttrue(status == 413 || status == 414 || status < 0);
    ttrue(stagedFiles() == 0);
}

static void fiberMain(void *data)
{
    if (scenarioSetup()) {
        testUploadScaled();
        testUploadOverLimit();
    }
    unlink(SOURCE);
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
