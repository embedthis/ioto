/*
    websocket-limits.tst.c - WebSocket frame and message limits and masking on the wire

    A client library masks its frames and stays within the limits, so these are sent by hand: an
    upgrade handshake followed by raw frames. The server must fail the connection with the
    protocol status (1002 for an unmasked frame, 1009 for a frame or assembled message over the
    configured limit) before buffering the payload, and must serve an ordinary request afterwards.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "test.h"

/*********************************** Locals ***********************************/

static char *HTTP;

#define FIN        0x80
#define OP_TEXT    0x1
#define OP_BINARY  0x2
#define OP_CONT    0x0
#define OP_CLOSE   0x8
#define MAX_FRAME  (100 * 1024)     //  web.json5 limits.maxFrame and limits.maxMessage

/************************************ Code ************************************/

/*
    Connect and complete the upgrade handshake on /test/ws/. Returns the socket or NULL.
 */
static RSocket *upgrade(void)
{
    RSocket *sp;
    cchar   *req = "GET /test/ws/ HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                   "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n"
                   "Sec-WebSocket-Protocol: chat\r\n\r\n";
    char    buf[2048];
    ssize   total, nbytes;

    if ((sp = rawConnect(HTTP)) == 0) {
        tfail("Cannot connect");
        return 0;
    }
    rWriteSocket(sp, req, slen(req), rGetTicks() + 5 * TPS);
    total = 0;
    buf[0] = '\0';
    while (total < (ssize) sizeof(buf) - 1) {
        nbytes = rReadSocket(sp, &buf[total], sizeof(buf) - 1 - (size_t) total, rGetTicks() + 3 * TPS);
        if (nbytes <= 0) {
            break;
        }
        total += nbytes;
        buf[total] = '\0';
        if (scontains(buf, "\r\n\r\n")) {
            break;
        }
    }
    if (!sstarts(buf, "HTTP/1.1 101")) {
        tfail("Upgrade refused: %.40s", buf);
        rFreeSocket(sp);
        return 0;
    }
    return sp;
}

/*
    Send refusal cases as headers only. Unread payload can cause a reset that discards the close frame on Windows.
 */
static void sendFrame(RSocket *sp, int first, bool masked, size_t len, cchar *payload, size_t payloadLen)
{
    static cuchar key[4] = { 0x11, 0x22, 0x33, 0x44 };
    uchar         *frame, *pp;
    size_t        i;

    frame = rAlloc(14 + payloadLen);
    pp = frame;
    *pp++ = (uchar) first;
    if (len <= 125) {
        *pp++ = (uchar) ((masked ? 0x80 : 0) | len);
    } else if (len <= 65535) {
        *pp++ = (uchar) ((masked ? 0x80 : 0) | 126);
        *pp++ = (uchar) (len >> 8);
        *pp++ = (uchar) (len & 0xff);
    } else {
        *pp++ = (uchar) ((masked ? 0x80 : 0) | 127);
        for (i = 8; i > 0; i--) {
            *pp++ = (uchar) ((len >> ((i - 1) * 8)) & 0xff);
        }
    }
    if (masked) {
        for (i = 0; i < 4; i++) {
            *pp++ = key[i];
        }
    }
    for (i = 0; i < payloadLen; i++) {
        *pp++ = masked ? (uchar) (payload[i] ^ key[i & 0x3]) : (uchar) payload[i];
    }
    rWriteSocket(sp, frame, (size_t) (pp - frame), rGetTicks() + 5 * TPS);
    rFree(frame);
}

/*
    Read the server's close frame and return its status, 0 if the socket closed without one, or
    -1 if nothing arrived.
 */
static int readClose(RSocket *sp)
{
    uchar buf[512];
    ssize total, nbytes;
    int   len;

    total = 0;
    while (total < (ssize) sizeof(buf)) {
        nbytes = rReadSocket(sp, (char*) &buf[total], sizeof(buf) - (size_t) total, rGetTicks() + 3 * TPS);
        if (nbytes <= 0) {
            break;
        }
        total += nbytes;
        if (total >= 2 && (buf[0] & 0x0f) == OP_CLOSE) {
            len = buf[1] & 0x7f;
            if (total >= 2 + len) {
                return len >= 2 ? (buf[2] << 8) | buf[3] : 0;
            }
        }
    }
    return total > 0 ? 0 : -1;
}

/*
    Read at least want bytes. A single read is not guaranteed to return a whole frame: the header
    and the payload may arrive in separate segments, and do on Windows. readClose already loops
    for exactly this reason.
 */
static ssize readAtLeast(RSocket *sp, uchar *buf, size_t bufsize, ssize want)
{
    ssize total, nbytes;

    total = 0;
    while (total < want && (size_t) total < bufsize) {
        nbytes = rReadSocket(sp, (char*) &buf[total], bufsize - (size_t) total, rGetTicks() + 3 * TPS);
        if (nbytes <= 0) {
            break;
        }
        total += nbytes;
    }
    return total;
}

static void checkServesAfterwards(void)
{
    Url  *up;
    char url[128];
    int  status;

    up = urlAlloc(0);
    status = urlFetch(up, "GET", SFMT(url, "%s/index.html", HTTP), NULL, 0, NULL);
    teqi(status, 200, "The server must serve an ordinary request afterwards");
    urlFree(up);
}

static void testUnmaskedFrameIsRefused(void)
{
    RSocket *sp;

    if ((sp = upgrade()) == 0) {
        return;
    }
    sendFrame(sp, FIN | OP_TEXT, 0, 2, NULL, 0);
    teqi(readClose(sp), 1002, "An unmasked client frame must close with 1002");
    rFreeSocket(sp);
    checkServesAfterwards();
}

static void testFrameOverLimitIsRefused(void)
{
    RSocket *sp;

    if ((sp = upgrade()) == 0) {
        return;
    }
    //  Declare a frame one byte over the limit; the refusal comes from the header alone
    sendFrame(sp, FIN | OP_BINARY, 1, MAX_FRAME + 1, NULL, 0);
    teqi(readClose(sp), 1009, "A frame over maxFrame must close with 1009");
    rFreeSocket(sp);
    checkServesAfterwards();
}

static void testFragmentedMessageOverLimitIsRefused(void)
{
    RSocket *sp;
    char    *payload;
    size_t  part;

    if ((sp = upgrade()) == 0) {
        return;
    }
    part = 60 * 1024;
    payload = rAlloc(part);
    memset(payload, 'm', part);
    /*
        Exceed maxMessage with two valid-size fragments. Send only the second header to avoid a reset.
     */
    sendFrame(sp, OP_BINARY, 1, part, payload, part);
    sendFrame(sp, FIN | OP_CONT, 1, part, NULL, 0);
    teqi(readClose(sp), 1009, "Fragments totalling more than maxMessage must close with 1009");
    rFree(payload);
    rFreeSocket(sp);
    checkServesAfterwards();
}

/*
    Sanity: a masked frame within the limits is echoed, so the refusals above are not an artefact
 */
static void testMaskedFrameIsEchoed(void)
{
    RSocket *sp;
    uchar   buf[64];
    ssize   nbytes;

    if ((sp = upgrade()) == 0) {
        return;
    }
    sendFrame(sp, FIN | OP_TEXT, 1, 5, "hello", 5);
    nbytes = readAtLeast(sp, buf, sizeof(buf), 7);
    ttrue(nbytes >= 7 && (buf[0] & 0x0f) == OP_TEXT && memcmp(&buf[2], "hello", 5) == 0, "A valid frame is echoed");
    sendFrame(sp, FIN | OP_CLOSE, 1, 2, "\x03\xe8", 2);
    rFreeSocket(sp);
}

static void fiberMain(void *arg)
{
    if (setup(&HTTP, NULL)) {
        testMaskedFrameIsEchoed();
        testUnmaskedFrameIsRefused();
        testFrameOverLimitIsRefused();
        testFragmentedMessageOverLimitIsRefused();
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
