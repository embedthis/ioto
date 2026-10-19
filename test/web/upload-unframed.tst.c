/*
    upload-unframed.tst.c - A multipart request with no framing headers must not be read as a body

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

#define BODY \
    "--XYZ\r\nContent-Disposition: form-data; name=\"f\"; filename=\"a.txt\"\r\n\r\n" \
    "DATA\r\n--XYZ--\r\n"

/************************************ Code ************************************/

static void testUnframedMultipartIsRefused(void)
{
    cchar *segments[1];
    char  buf[8192];
    int   status;

    segments[0] = "POST /test/show HTTP/1.1\r\nHost: x\r\n"
                  "Content-Type: multipart/form-data; boundary=XYZ\r\n\r\n" BODY;
    status = rawRequest(HTTP, segments, 1, 0, 2, buf, sizeof(buf));
    teqi(status, 411, "An unframed multipart request must be refused");
    teqi(rawCountResponses(buf, (ssize) slen(buf)), 1, "Only the refusal may be sent");
    tfalse(scontains(buf, "a.txt") != 0, "The unframed body must not be processed as an upload");
}

static void testFramedMultipartIsAccepted(void)
{
    cchar *segments[1];
    char  buf[8192], *req;
    int   status;

    req = sfmt("POST /test/show HTTP/1.1\r\nHost: x\r\n"
               "Content-Type: multipart/form-data; boundary=XYZ\r\n"
               "Content-Length: %d\r\nConnection: close\r\n\r\n%s", (int) slen(BODY), BODY);
    segments[0] = req;
    status = rawRequest(HTTP, segments, 1, 0, 1, buf, sizeof(buf));
    teqi(status, 200, "A multipart request with Content-Length must still be accepted");
    rFree(req);
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        testUnframedMultipartIsRefused();
        testFramedMultipartIsAccepted();
    }
    rFree(HTTP);
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
