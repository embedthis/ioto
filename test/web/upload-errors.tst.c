/*
    upload-errors.tst.c - Malformed multipart uploads

    Each request here is a multipart body a client library would never produce: a missing
    boundary parameter, a part with no name, junk after a boundary, a body that ends inside the
    part headers, a form field larger than the parser will buffer, and a field with no data at
    all. Each must be refused with a client error, promptly, and never crash or stall the server.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

#define BOUNDARY  "----UploadErrorBoundary"
#define REQUEST   "POST /test/show HTTP/1.1\r\nHost: x\r\n"
#define MULTIPART "Content-Type: multipart/form-data; boundary=" BOUNDARY "\r\n"

/************************************ Code ************************************/

/*
    POST a multipart body with an exact Content-Length and return the status
 */
static int upload(cchar *headers, cchar *body, char *buf, size_t size)
{
    cchar *segments[1];
    char  *req;
    int   status;

    /*
        One write, not two. A request the server refuses on its headers is answered and the
        connection closed while a second write is still in flight, and on Windows that write to a
        closed socket produces an RST which discards the buffered response - so the status arrives
        as 0 rather than the refusal being tested. Sending the whole request at once removes the
        race. The server's missing lingering close is web issue 60.

        Built on the heap: one case here sends a 70KB field deliberately, so no fixed buffer will do.
     */
    req = sfmt(REQUEST "%sContent-Length: %d\r\n\r\n%s", headers, (int) slen(body), body);
    segments[0] = req;
    status = rawRequest(HTTP, segments, 1, 0, 1, buf, size);
    rFree(req);
    return status;
}

static void testMissingBoundaryParameter(void)
{
    char buf[4096];
    int  status;

    status = upload("Content-Type: multipart/form-data\r\n", "--x\r\n\r\n--x--\r\n", buf, sizeof(buf));
    teqi(status, 400, "A multipart type without a boundary must be refused");
}

static void testPartWithoutName(void)
{
    char buf[4096];
    int  status;

    status = upload(MULTIPART,
                    "--" BOUNDARY "\r\nContent-Disposition: form-data; filename=\"x.txt\"\r\n\r\n"
                    "data\r\n--" BOUNDARY "--\r\n", buf, sizeof(buf));
    teqi(status, 400, "A file part without a name must be refused");
}

static void testJunkAfterBoundary(void)
{
    char buf[4096];
    int  status;

    status = upload(MULTIPART, "--" BOUNDARY "xx\r\n\r\n--" BOUNDARY "--\r\n", buf, sizeof(buf));
    teqi(status, 400, "A boundary followed by junk must be refused");

    //  Longer than the suffix the parser will read, and only then terminated
    status = upload(MULTIPART, "--" BOUNDARY "junk-junk-junk-junk-junk\r\n\r\n--" BOUNDARY "--\r\n", buf, sizeof(buf));
    teqi(status, 400, "A boundary followed by an over-long line must be refused");
}

static void testBodyEndsInsidePartHeaders(void)
{
    char  buf[4096];
    Ticks started;
    int   status;

    started = rGetTicks();
    status = upload(MULTIPART, "--" BOUNDARY "\r\nContent-Disposition: form-data; name=\"a\"\r\n", buf, sizeof(buf));
    teqi(status, 400, "A body that ends inside the part headers must be refused");
    ttrue(rGetTicks() - started < 2 * TPS, "The refusal must not wait for a socket timeout");
}

/*
    A form field is buffered whole, so one larger than the buffer limit is refused rather than
    left to grow
 */
static void testFormFieldTooBig(void)
{
    char  *body, buf[4096];
    cchar *head, *tail;
    size_t headLen, tailLen, valueLen;
    int   status;

    head = "--" BOUNDARY "\r\nContent-Disposition: form-data; name=\"big\"\r\n\r\n";
    tail = "\r\n--" BOUNDARY "--\r\n";
    headLen = slen(head);
    tailLen = slen(tail);
    valueLen = 70 * 1024;
    body = rAlloc(headLen + valueLen + tailLen + 1);
    scopy(body, headLen + 1, head);
    memset(&body[headLen], 'v', valueLen);
    scopy(&body[headLen + valueLen], tailLen + 1, tail);

    status = upload(MULTIPART, body, buf, sizeof(buf));
    teqi(status, 414, "A form field larger than the buffer limit must be refused");
    rFree(body);
}

static void testFormFieldWithoutData(void)
{
    char buf[4096];
    int  status;

    status = upload(MULTIPART, "--" BOUNDARY "\r\nContent-Disposition: form-data; name=\"a\"\r\n\r\n--" BOUNDARY "--\r\n",
                    buf, sizeof(buf));
    teqi(status, 400, "A form field whose boundary follows the headers directly must be refused");
}

/*
    Sanity: the same shape, well formed, is accepted. Guards against the tests above passing
    because uploads are broken altogether.
 */
static void testWellFormedFieldIsAccepted(void)
{
    char buf[8192];
    int  status;

    status = upload(MULTIPART, "--" BOUNDARY "\r\nContent-Disposition: form-data; name=\"color\"\r\n\r\nblue\r\n--" BOUNDARY "--\r\n",
                    buf, sizeof(buf));
    teqi(status, 200);
    tcontains(buf, "\"color\":\"blue\"");
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        testMissingBoundaryParameter();
        testPartWithoutName();
        testJunkAfterBoundary();
        testBodyEndsInsidePartHeaders();
        testFormFieldTooBig();
        testFormFieldWithoutData();
        testWellFormedFieldIsAccepted();
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
