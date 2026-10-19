/*
    websocket-key.tst.c - Sec-WebSocket-Key length and charset

    addHeaders concatenated the client's Sec-WebSocket-Key with the protocol GUID into a fixed
    128-byte stack buffer. sjoinbuf refuses rather than truncates and leaves the destination
    untouched, and the return was discarded, so a key of 128 bytes or more left the buffer never
    written: the SHA-1 then ran over uninitialised stack - past the end of the buffer when the stale
    bytes held no NUL - and its digest was base64-encoded into Sec-WebSocket-Accept, which made the
    accept token both an oracle over the stack and identical for every over-long key.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

//  RFC 6455 section 1.3 worked example
#define RFC_KEY    "dGhlIHNhbXBsZSBub25jZQ=="
#define RFC_ACCEPT "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="

/************************************ Code ************************************/

/*
    Attempt an upgrade with the given key. Returns the status; the accept header is copied out.
 */
static int upgrade(cchar *key, char *accept, size_t size)
{
    Url   *up;
    cchar *header;
    char  url[256];
    int   status;

    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/ws/", HTTP), NULL, 0,
                      "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                      "Sec-WebSocket-Version: 13\r\nSec-WebSocket-Key: %s\r\n", key);
    header = urlGetHeader(up, "Sec-WebSocket-Accept");
    scopy(accept, size, header ? header : "");
    urlFree(up);
    return status;
}

/*
    Only a key of exactly 24 base64 characters is a key.
 */
static void testKeyLengths(void)
{
    char *key, accept[256];
    int  i, status;
    static int lengths[] = { 1, 23, 25, 127, 128, 4096, 0 };

    status = upgrade(RFC_KEY, accept, sizeof(accept));
    teqi(status, 101, "A well-formed key must be accepted");
    tmatch(accept, RFC_ACCEPT);

    for (i = 0; lengths[i]; i++) {
        key = rAlloc((size_t) lengths[i] + 1);
        memset(key, 'A', (size_t) lengths[i]);
        key[lengths[i]] = '\0';
        status = upgrade(key, accept, sizeof(accept));
        teqi(status, 400, "A key of %d characters must be refused", lengths[i]);
        ttrue(accept[0] == '\0', "A refused key must not compose an accept header, got '%s'", accept);
        rFree(key);
    }
}

/*
    A 24-character value that is not base64 is not a key either.
 */
static void testKeyCharset(void)
{
    char accept[256];
    int  status;

    status = upgrade("........................", accept, sizeof(accept));
    teqi(status, 400, "A 24-character non-base64 key must be refused");

    status = upgrade("dGhlIHNhbXBsZSBub25j\"'<>", accept, sizeof(accept));
    teqi(status, 400, "A key carrying quoting characters must be refused");
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        testKeyLengths();
        testKeyCharset();
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
