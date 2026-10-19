/*
    upload-total.tst.c - web.limits.upload bounds the whole request for every framing

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

#define ENDPOINT "http://127.0.0.1:4876"
#define BOUNDARY "----UploadTotalBoundary"

/************************************ Code ************************************/

/*
    POST count chunked file parts of size bytes each and return the status
 */
static int postChunked(int count, int size)
{
    cchar *segments[16];
    char  *last, *part, *parts[8], *data, response[4096];
    int   i, n, status;

    data = rAlloc((size_t) size + 1);
    memset(data, 'D', (size_t) size);
    data[size] = '\0';

    n = 0;
    segments[n++] = "POST /test/show HTTP/1.1\r\nHost: x\r\nConnection: close\r\n"
                    "Transfer-Encoding: chunked\r\n"
                    "Content-Type: multipart/form-data; boundary=" BOUNDARY "\r\n\r\n";
    for (i = 0; i < count; i++) {
        part = sfmt("--" BOUNDARY "\r\nContent-Disposition: form-data; name=\"f%d\"; filename=\"t%d.bin\"\r\n\r\n"
                    "%s\r\n", i, i, data);
        parts[i] = sfmt("%zx\r\n%s\r\n", slen(part), part);
        rFree(part);
        segments[n++] = parts[i];
    }
    last = sfmt("%zx\r\n--" BOUNDARY "--\r\n\r\n0\r\n\r\n", slen("--" BOUNDARY "--\r\n"));
    segments[n++] = last;

    status = rawRequest(ENDPOINT, segments, n, 0, 1, response, sizeof(response));
    for (i = 0; i < count; i++) {
        rFree(parts[i]);
    }
    rFree(last);
    rFree(data);
    return status;
}

static void testChunkedTotal(void)
{
    WebHost *host;
    int     status;

    host = startTestHost("{ web: { documents: './site', listen: ['" ENDPOINT "'], "
                         "limits: { body: '8K', upload: '1K' }, "
                         "routes: [ { match: '/test/', handler: 'action' }, {} ] } }");
    if (!host) {
        return;
    }
    status = postChunked(2, 400);
    teqi(status, 200, "Files totalling less than web.limits.upload are accepted");

    status = postChunked(3, 700);
    teqi(status, 413, "Files totalling more than web.limits.upload must be refused whatever the framing");
    stopTestHost(host);
}

static void fiberMain(void *data)
{
    testChunkedTotal();
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
