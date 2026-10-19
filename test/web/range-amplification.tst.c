/*
    range-amplification.tst.c - Range count limit and coalescing (web issue 12)

    Every range in a Range header used to be served in full, so a 7 KB header repeating "0-99"
    fifteen hundred times over a 100-byte file produced a 327 KB response: a 3000x amplifier
    available to any anonymous client. The header is now bounded in range count (past the cap it
    is ignored and the document served whole, which the file size bounds) and overlapping or
    adjacent ranges are coalesced, so the bytes served never exceed the file.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

/************************************ Code ************************************/

/*
    GET range-test.txt (100 bytes) with the given Range value over a raw socket, Connection: close
    so the whole response can be read to EOF. Returns the status and the body size.
 */
static int rangeProbe(cchar *rangeValue, ssize *bodyBytes, char *headers, size_t size)
{
    RSocket *sp;
    RBuf    *rb;
    char    buf[8192], *bodyStart;
    cchar   *req;
    ssize   nbytes, total, headerLen;
    int     status;

    *bodyBytes = 0;
    *headers = '\0';
    if ((sp = rawConnect(HTTP)) == 0) {
        return -1;
    }
    rb = rAllocBuf(0);
    rPutToBuf(rb, "GET /range-test.txt HTTP/1.1\r\nHost: x\r\nConnection: close\r\nRange: %s\r\n\r\n", rangeValue);
    req = rBufToString(rb);
    rWriteSocket(sp, req, slen(req), rGetTicks() + 5 * TPS);
    rFreeBuf(rb);

    total = 0;
    status = -1;
    headerLen = -1;
    while (total < (ssize) sizeof(buf) - 1 &&
           (nbytes = rReadSocket(sp, &buf[total], sizeof(buf) - 1 - (size_t) total,
                                 rGetTicks() + 10 * TPS)) > 0) {
        total += nbytes;
        buf[total] = '\0';
        if (status < 0 && total > 12) {
            status = (int) stoi(&buf[9]);
        }
        if (headerLen < 0 && (bodyStart = scontains(buf, "\r\n\r\n")) != 0) {
            headerLen = (bodyStart + 4) - buf;
        }
    }
    rFreeSocket(sp);
    sncopy(headers, size, buf, (size_t) total);
    *bodyBytes = (headerLen >= 0) ? total - headerLen : total;
    return status;
}

/*
    Count the multipart parts in a response by their Content-Range lines
 */
static int countParts(cchar *response)
{
    cchar *cp;
    int   count;

    for (count = 0, cp = response; (cp = scontains(cp, "Content-Range: bytes ")) != 0; cp++) {
        count++;
    }
    return count;
}

/*
    Build "0-99,0-99,..." with count entries
 */
static char *repeatedRange(int count)
{
    RBuf *rb;
    char *result;
    int  i;

    rb = rAllocBuf(0);
    rPutStringToBuf(rb, "bytes=");
    for (i = 0; i < count; i++) {
        rPutToBuf(rb, "%s0-99", i ? "," : "");
    }
    result = sclone(rGetBufStart(rb));
    rFreeBuf(rb);
    return result;
}

static void testRangeAmplification(void)
{
    char  *value, headers[8192];
    ssize body;
    int   status;

    value = repeatedRange(500);
    status = rangeProbe(value, &body, headers, sizeof(headers));
    tneqi(status, 206, "500 overlapping ranges must not return 206 multipart");
    tltez(body, 1000, "The served body must be bounded near the file size, not amplified");
    rFree(value);

    //  Past the cap the header is ignored and the whole document served
    value = repeatedRange(WEB_MAX_RANGES + 1);
    status = rangeProbe(value, &body, headers, sizeof(headers));
    teqi(status, 200, "More ranges than the cap serves the whole document");
    teqz(body, 100);
    rFree(value);

    //  At the cap, identical ranges coalesce to one and the response is a single range
    value = repeatedRange(WEB_MAX_RANGES);
    status = rangeProbe(value, &body, headers, sizeof(headers));
    teqi(status, 206, "Ranges within the cap are honoured");
    tcontains(headers, "Content-Range: bytes 0-99/100");
    teqz(body, 100);
    rFree(value);
}

static void testRangeCoalescing(void)
{
    char  headers[8192];
    ssize body;
    int   status;

    status = rangeProbe("bytes=0-49,25-75", &body, headers, sizeof(headers));
    teqi(status, 206);
    tcontains(headers, "Content-Range: bytes 0-75/100");
    teqz(body, 76);

    status = rangeProbe("bytes=50-99,0-49", &body, headers, sizeof(headers));
    teqi(status, 206, "Adjacent ranges given out of order coalesce");
    tcontains(headers, "Content-Range: bytes 0-99/100");
    teqz(body, 100);

    status = rangeProbe("bytes=0-9,20-29", &body, headers, sizeof(headers));
    teqi(status, 206, "Disjoint ranges are still served as multipart");
    tcontains(headers, "multipart/byteranges");

    status = rangeProbe("bytes=20-29,0-9,20-29", &body, headers, sizeof(headers));
    teqi(status, 206, "Duplicates collapse and the parts are served in order");
    tcontains(headers, "multipart/byteranges");
    teqi(countParts(headers), 2, "Two parts, not three");
    ttrue(scontains(headers, "bytes 0-9/100") < scontains(headers, "bytes 20-29/100"), "Parts are in offset order");
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        testRangeAmplification();
        testRangeCoalescing();
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
