/*
    upload-filenames.tst.c - Upload client filename validation

    The client filename in a multipart Content-Disposition header is attacker controlled. The
    upload handler must refuse anything that could escape the upload directory or name a file
    outside it, and it must do so before creating a temp file.

    An upload is stored under a generated temp name and only the test action copies it to
    ./tmp/<clientFilename>, so a rejected upload is observable both as a 400 and as the absence
    of any file.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;
static char *HTTPS;

#define BOUNDARY "----WebKitFormBoundaryUploadNameTest"

/************************************ Code ************************************/

/*
    POST a single-part multipart upload declaring the given client filename. Returns the status.
 */
static int upload(cchar *clientFilename)
{
    Url  *up;
    char url[128], headers[256], body[1024];
    int  status;

    up = urlAlloc(0);
    SFMT(body,
         "--%s\r\n"
         "Content-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"
         "Content-Type: text/plain\r\n"
         "\r\n"
         "payload\r\n"
         "--%s--\r\n",
         BOUNDARY, clientFilename, BOUNDARY);
    SFMT(headers, "Content-Type: multipart/form-data; boundary=%s\r\n", BOUNDARY);

    status = urlFetch(up, "POST", SFMT(url, "%s/test/upload/", HTTP), body, slen(body), headers);
    urlFree(up);
    return status;
}

/*
    Upload and assert the status. The upload must be made exactly once: teqi evaluates its first
    argument twice, once to compare and once to report.
 */
static void checkUpload(cchar *clientFilename, int expect)
{
    int status;

    status = upload(clientFilename);
    teqi(status, expect);
}

/*
    A filename must be rejected and must leave nothing behind anywhere the traversal aimed at.
 */
static void refuse(cchar *clientFilename, cchar *comment)
{
    int status;

    status = upload(clientFilename);
    if (status == 200) {
        rError("web", "Upload of '%s' was accepted (%s)", clientFilename, comment);
    }
    teqi(status, 400);
}

/*
    A plain filename is accepted, so the rejections below are attributable to the name and not
    to the request framing.
 */
static void testAcceptsPlainName(void)
{
    char filepath[256];
    char name[64];
    int  pid;

    pid = getpid();
    SFMT(name, "plain-%d.txt", pid);
    checkUpload(name, 200);

    SFMT(filepath, "tmp/%s", name);
    ttrue(rFileExists(filepath));
    unlink(filepath);
}

/*
    Relative traversal, in raw and percent-encoded forms.
 */
static void testRejectsTraversal(void)
{
    refuse("../escape.txt", "parent directory");
    refuse("../../escape.txt", "two levels up");
    refuse("a/../../escape.txt", "traversal after a segment");
    refuse("..", "bare parent");
    refuse(".", "bare current directory");
    refuse(".hidden", "leading dot");

    //  Percent-encoded "." and "/" must not slip past the literal check
    refuse("%2e%2e/escape.txt", "encoded dot-dot");
    refuse("%2E%2E/escape.txt", "encoded dot-dot upper case");
    refuse("a%2fb.txt", "encoded forward slash");
    refuse("a%2Fb.txt", "encoded forward slash upper case");
    refuse("a%5cb.txt", "encoded backslash");
    refuse("a%5Cb.txt", "encoded backslash upper case");
}

/*
    A path separator of any kind names a file outside the upload directory.
 */
static void testRejectsSeparators(void)
{
    refuse("/etc/passwd", "absolute path");
    refuse("sub/dir.txt", "forward slash");
    refuse("sub\\dir.txt", "backslash");
    refuse("C:file.txt", "drive letter");
}

/*
    Shell and glob metacharacters, and anything that could break a later quoted use of the name.
 */
static void testRejectsMetacharacters(void)
{
    refuse("star*.txt", "glob star");
    refuse("quest?.txt", "glob question");
    refuse("lt<.txt", "redirect in");
    refuse("gt>.txt", "redirect out");
    refuse("pipe|.txt", "pipe");
    refuse("tilde~.txt", "tilde");
    refuse("quote\\\".txt", "double quote");
    refuse("back`tick.txt", "backtick");
    refuse("caret^.txt", "caret");
}

/*
    An empty filename has no valid interpretation.
 */
static void testRejectsEmptyName(void)
{
    refuse("", "empty filename");
}

static void fiberMain(void *data)
{
    if (setup(&HTTP, &HTTPS)) {
        testAcceptsPlainName();
        testRejectsTraversal();
        testRejectsSeparators();
        testRejectsMetacharacters();
        testRejectsEmptyName();
    }
    rFree(HTTP);
    rFree(HTTPS);
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
