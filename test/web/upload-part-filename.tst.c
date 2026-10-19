/*
    upload-part-filename.tst.c - One file per multipart part

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

#define BOUNDARY  "----PartFilenameBoundary"
#define REQUEST   "POST /test/show HTTP/1.1\r\nHost: x\r\nConnection: close\r\n"
#define MULTIPART "Content-Type: multipart/form-data; boundary=" BOUNDARY "\r\n"
#define PART      "--" BOUNDARY "\r\n"
#define LAST      "--" BOUNDARY "--\r\n"

/************************************ Code ************************************/

static int upload(cchar *body, char *buf, size_t size)
{
    cchar *segments[1];
    char  *req;
    int   status;

    req = sfmt(REQUEST MULTIPART "Content-Length: %d\r\n\r\n%s", (int) slen(body), body);
    segments[0] = req;
    status = rawRequest(HTTP, segments, 1, 0, 1, buf, size);
    rFree(req);
    return status;
}

static void testTwoFilenamesRefused(void)
{
    char buf[8192];
    int  status;

    status = upload(PART "Content-Disposition: form-data; name=\"f\"; filename=\"a.txt\"; filename=\"b.txt\"\r\n\r\n"
                    "x\r\n" LAST, buf, sizeof(buf));
    teqi(status, 400, "A part naming two files must be refused");
}

static void testOneFileAccepted(void)
{
    char buf[8192];
    int  status;

    status = upload(PART "Content-Disposition: form-data; name=\"f\"; filename=\"a.txt\"\r\n\r\nx\r\n" LAST,
                    buf, sizeof(buf));
    teqi(status, 200);
}

static void testContentTypeStaysWithItsPart(void)
{
    char buf[8192];
    int  status;

    status = upload(PART "Content-Disposition: form-data; name=\"f\"; filename=\"a.txt\"\r\n"
                    "Content-Type: image/png\r\n\r\nhello\r\n"
                    PART "Content-Disposition: form-data; name=\"v\"\r\n"
                    "Content-Type: application/x-other\r\n\r\nval\r\n" LAST, buf, sizeof(buf));
    teqi(status, 200);
    tcontains(buf, "\"contentType\":\"image/png\"");
    tfalse(scontains(buf, "application/x-other"), "A later part's Content-Type must not land on an earlier upload");

    //  Header order within a part does not matter
    status = upload(PART "Content-Type: image/png\r\n"
                    "Content-Disposition: form-data; name=\"f\"; filename=\"a.txt\"\r\n\r\nhello\r\n" LAST,
                    buf, sizeof(buf));
    teqi(status, 200);
    tcontains(buf, "\"contentType\":\"image/png\"");
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        testTwoFilenamesRefused();
        testOneFileAccepted();
        testContentTypeStaysWithItsPart();
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
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
