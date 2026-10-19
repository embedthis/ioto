/*
    upload-count.tst.c - The per-request upload file count limit

    web.limits.uploads bounds the number of files one multipart request may create. It used to
    default to 0, and the check was written as "> 0 && ...", so at the shipped default the limit
    did not exist. These tests run against the shared server, whose web.json5 does not
    set the key, so they exercise the default itself.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

/*
    The web.limits.uploads default in src/host.c. Not configured in test/web.json5, so this is
    what the server is running with.
 */
#define UPLOAD_LIMIT 50

static cchar *BOUNDARY = "----WebKitFormBoundaryUploadCount";

/************************************ Code ************************************/

/*
    Build a multipart body of count file parts, each with its own filename.
 */
static RBuf *buildBody(cchar *prefix, int pid, int count)
{
    RBuf *buf;
    int  i;

    buf = rAllocBuf(count * 256);
    for (i = 0; i < count; i++) {
        rPutToBuf(buf,
                  "--%s\r\n"
                  "Content-Disposition: form-data; name=\"file%d\"; filename=\"%s-%d-%d.txt\"\r\n"
                  "Content-Type: text/plain\r\n"
                  "\r\n"
                  "part %d\r\n",
                  BOUNDARY, i, prefix, pid, i, i);
    }
    rPutToBuf(buf, "--%s--\r\n", BOUNDARY);
    return buf;
}

static int postFiles(cchar *prefix, int pid, int count)
{
    Url  *up;
    RBuf *body;
    char url[128], headers[256];
    int  status;

    body = buildBody(prefix, pid, count);
    SFMT(headers, "Content-Type: multipart/form-data; boundary=%s\r\n", BOUNDARY);

    up = urlAlloc(0);
    status = urlFetch(up, "POST", SFMT(url, "%s/test/upload/", HTTP),
                      rGetBufStart(body), rGetBufLength(body), headers);
    urlFree(up);
    rFreeBuf(body);
    return status;
}

/*
    Remove whatever the upload action wrote, and report how many of the parts arrived.
 */
static int reapFiles(cchar *prefix, int pid, int count)
{
    char path[256];
    int  found, i;

    found = 0;
    for (i = 0; i < count; i++) {
        SFMT(path, "tmp/%s-%d-%d.txt", prefix, pid, i);
        if (rFileExists(path)) {
            found++;
            unlink(path);
        }
    }
    return found;
}

/*
    A request at the limit is accepted and every file arrives. This pins the limit's arithmetic:
    counting parts rather than files, or counting the terminating boundary, refuses this body.
 */
static void testUploadsAtLimit(void)
{
    int pid, status;

    pid = getpid();
    status = postFiles("count-ok", pid, UPLOAD_LIMIT);
    teqi(status, 200);
    teqi(reapFiles("count-ok", pid, UPLOAD_LIMIT), UPLOAD_LIMIT);
}

/*
    One file past the limit is refused, at the shipped default and with nothing configured.
 */
static void testUploadsOverLimit(void)
{
    int pid, status;

    pid = getpid();
    status = postFiles("count-over", pid, UPLOAD_LIMIT + 1);
    teqi(status, 413);

    //  The request failed, so the action never ran and no part reached the upload directory
    teqi(reapFiles("count-over", pid, UPLOAD_LIMIT + 1), 0);
}

/*
    Form fields are not files: they open no descriptor and consume no upload-directory inode, so
    they are bounded by web.limits.vars instead and must not count against this limit.
 */
static void testFormFieldsDoNotCount(void)
{
    Url  *up;
    RBuf *body;
    char url[128], headers[256], path[256];
    int  i, pid, status;

    pid = getpid();
    body = rAllocBuf(4096);
    for (i = 0; i < UPLOAD_LIMIT * 2; i++) {
        rPutToBuf(body,
                  "--%s\r\n"
                  "Content-Disposition: form-data; name=\"field%d\"\r\n"
                  "\r\n"
                  "value %d\r\n",
                  BOUNDARY, i, i);
    }
    rPutToBuf(body,
              "--%s\r\n"
              "Content-Disposition: form-data; name=\"file\"; filename=\"count-fields-%d.txt\"\r\n"
              "Content-Type: text/plain\r\n"
              "\r\n"
              "one file\r\n"
              "--%s--\r\n",
              BOUNDARY, pid, BOUNDARY);

    SFMT(headers, "Content-Type: multipart/form-data; boundary=%s\r\n", BOUNDARY);
    up = urlAlloc(0);
    status = urlFetch(up, "POST", SFMT(url, "%s/test/upload/", HTTP),
                      rGetBufStart(body), rGetBufLength(body), headers);
    teqi(status, 200);
    urlFree(up);
    rFreeBuf(body);

    SFMT(path, "tmp/count-fields-%d.txt", pid);
    ttrue(rFileExists(path));
    unlink(path);
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, NULL)) {
        testUploadsAtLimit();
        testUploadsOverLimit();
        testFormFieldsDoNotCount();
    }
    rFree(HTTP);
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
