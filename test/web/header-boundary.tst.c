/*
    header-boundary.tst.c - Header blocks sized exactly to the rxHeaders buffer capacity

    parseHeaders copies the header block into web->rxHeaders and then tokenises the request line in
    place, so web->method, web->url and web->protocol are raw pointers into that buffer. When the
    block filled the buffer exactly, rPutBlockToBuf skipped its trailing NUL and the deferred
    rAddNullToBuf grew the buffer - reallocating and freeing the storage those three pointers named.
    The freed bytes were then validated as the URL and emitted as the response status line, so the
    reply began with garbage instead of "HTTP/1.1".

    The trigger is the exact capacity, so each size is driven with its neighbours as controls.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

/************************************ Code ************************************/

/*
    Send a request whose header block - everything up to and including the terminating CRLF CRLF,
    less that final CRLF - is exactly "block" bytes, by padding an X-Pad header. Returns the raw
    response in buf.
 */
static void requestWithBlock(size_t block, char *buf, size_t size)
{
    cchar  *segments[1];
    char   *req, *pad;
    size_t fixed, padLen;

    /*
        "GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\nX-Pad: " + pad + "\r\n" is the block,
        and the request adds the final "\r\n".
     */
    fixed = slen("GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\nX-Pad: \r\n");
    if (block <= fixed) {
        tfail("Block size %d is too small", (int) block);
        return;
    }
    padLen = block - fixed;
    pad = rAlloc(padLen + 1);
    memset(pad, 'A', padLen);
    pad[padLen] = '\0';

    req = sfmt("GET / HTTP/1.1\r\nHost: x\r\nConnection: close\r\nX-Pad: %s\r\n\r\n", pad);
    segments[0] = req;
    rawRequest(HTTP, segments, 1, 0, 1, buf, size);
    rFree(pad);
    rFree(req);
}

/*
    Every size must produce a well-formed status line. A block at the buffer capacity used to
    return one built from freed memory.
 */
static void checkBlock(size_t block)
{
    char buf[16384];

    buf[0] = '\0';
    requestWithBlock(block, buf, sizeof(buf));
    ttrue(sstarts(buf, "HTTP/1.1 "), "A %d-byte header block must yield an HTTP/1.1 status line, got '%.16s'",
          (int) block, buf);
}

static void testCapacityBoundary(void)
{
    //  ME_BUFSIZE and one growth past it, each with its neighbours as controls
    checkBlock(4095);
    checkBlock(4096);
    checkBlock(4097);
    checkBlock(8191);
    checkBlock(8192);
    checkBlock(8193);
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        testCapacityBoundary();
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
