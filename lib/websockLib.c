/*
 * Embedthis WebSockets Library Source
 */

#include "websock.h"

#if ME_COM_WEBSOCK



/********* Start of file src/websockLib.c ************/

/*
    websockLib.c - Web Sockets

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/


#include    "crypt.h"

/********************************** Locals ************************************/
#if ME_COM_WEBSOCK
/*
    Message frame states
 */
#define WS_BEGIN 0
#define WS_MSG   1

/*
   static char *codetxt[16] = {
    "cont", "text", "binary", "reserved", "reserved", "reserved", "reserved", "reserved",
    "close", "ping", "pong", "reserved", "reserved", "reserved", "reserved", "reserved",
   }; */

/*
    Frame format

     Byte 0          Byte 1          Byte 2          Byte 3
     0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 +-+-+-+-+-------+-+-------------+-------------------------------+
 |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
 |I|S|S|S|  (4)  |A|     (7)     |             (16/63)           |
 |N|V|V|V|       |S|             |   (if payload len==126/127)   |
 | |1|2|3|       |K|             |                               |
 +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
 |     Extended payload length continued, if payload len == 127  |
 + - - - - - - - - - - - - - - - +-------------------------------+
 |                               |Masking-key, if MASK set to 1  |
 +-------------------------------+-------------------------------+
 | Masking-key (continued)       |          Payload Data         |
 +-------------------------------- - - - - - - - - - - - - - - - +
    :                     Payload Data continued ...                :
 + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
 |                     Payload Data continued ...                |
 +---------------------------------------------------------------+

    Single message has
        fin == 1
    Fragmented message has
        fin == 0, opcode != 0
        fin == 0, opcode == 0
        fin == 1, opcode == 0

    Common first byte codes:
        0x9B    Fin | /SET

    NOTE: control frames (opcode >= 8) can be sent between fragmented frames
 */
#define GET_FIN(v)      (((v) >> 7) & 0x1)                  /* Final fragment */
#define GET_RSV(v)      (((v) >> 4) & 0x7)                  /* Reserved (used for extensions) */
#define GET_CODE(v)     ((v) & 0xf)                         /* Packet opcode */
#define GET_MASK(v)     (((v) >> 7) & 0x1)                  /* True if dataMask in frame (client send) */
#define GET_LEN(v)      ((v) & 0x7f)                        /* Low order 7 bits of length */

#define SET_FIN(v)      (((v) & 0x1) << 7)
#define SET_MASK(v)     (((v) & 0x1) << 7)
#define SET_CODE(v)     ((v) & 0xf)
#define SET_LEN(len, n) ((uchar) (((len) >> ((n) * 8)) & 0xff))

#define UTF8_ACCEPT 0
#define UTF8_REJECT 1

static const uchar utfTable[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 00..1f
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 20..3f
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 40..5f
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 60..7f
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, // 80..9f
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, // a0..bf
    8, 8, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // c0..df
    0xa, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x4, 0x3, 0x3,                 // e0..ef
    0xb, 0x6, 0x6, 0x6, 0x5, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8,                 // f0..ff
    0x0, 0x1, 0x2, 0x3, 0x5, 0x8, 0x7, 0x1, 0x1, 0x1, 0x4, 0x6, 0x1, 0x1, 0x1, 0x1,                 // s0..s0
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, // s1..s2
    1, 2, 1, 1, 1, 1, 1, 2, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, // s3..s4
    1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1, 3, 1, 1, 1, 1, 1, 1, // s5..s6
    1, 3, 1, 1, 1, 1, 1, 3, 1, 3, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // s7..s8
};

/********************************** Forwards **********************************/

static void invokeCallback(WebSocket *ws, int event, cchar *buf, size_t len);
static int parseMessage(WebSocket *ws);
static int parseFrame(WebSocket *ws);
static uint validUTF8(WebSocket *ws, cchar *str, size_t len);
static bool validateText(WebSocket *ws);
static int writeFrame(WebSocket *ws, int type, int fin, cuchar *buf, size_t len);
static int wsError(WebSocket *ws, int code, cchar *fmt, ...);

/*********************************** Code *************************************/

PUBLIC WebSocket *webSocketAlloc(RSocket *sock, bool client)
{
    WebSocket *ws;

    if (!sock) {
        return 0;
    }
    if ((ws = rAllocType(WebSocket)) == 0) {
        rFatal("sockets", "memory error");
        return 0;
    }
    ws->sock = sock;
    ws->client = client;
    ws->state = WS_STATE_CONNECTING;
    ws->closeStatus = WS_STATUS_NO_STATUS;
    ws->maxFrame = WS_MAX_FRAME;
    ws->maxMessage = WS_MAX_MESSAGE;
    ws->validate = 1;
    ws->buf = rAllocBuf(ME_BUFSIZE);
    return ws;
}

PUBLIC void webSocketFree(WebSocket *ws)
{
    if (!ws) return;

    rFreeBuf(ws->buf);
    rFree(ws->clientKey);
    rFree(ws->closeReason);
    rFree(ws->errorMessage);
    rFree(ws->protocol);
    rFree(ws);
}

/*
    Read data from the socket into the WebSocket buffer
    Return the number of bytes read, 0 if no data available, < 0 on error
 */
static ssize readSocket(WebSocket *ws)
{
    RBuf   *bp;
    size_t toRead;
    ssize  nbytes;

    bp = ws->buf;
    //  Compact if empty or insufficient space
    if (rGetBufLength(bp) == 0 || rGetBufSpace(bp) < ME_BUFSIZE) {
        rCompactBuf(bp);
    }
    rReserveBufSpace(bp, ME_BUFSIZE);
    toRead = rGetBufSpace(bp);

    if ((nbytes = rReadSocket(ws->sock, bp->end, toRead, ws->deadline)) < 0) {
        return wsError(ws, 0, "Cannot read from socket");
    }
    rAdjustBufEnd(bp, nbytes);
    return nbytes;
}

static void invokeCallback(WebSocket *ws, int event, cchar *buf, size_t len)
{
    if (ws->callback) {
        ws->callback(ws, event, buf, len, ws->callbackArg);
    }
}

/*
    Run the WebSocket event loop until the connection closes.
    Single-fiber model - no coordination with other fibers needed.
    Returns 0 on orderly close, < 0 on error.
 */
PUBLIC int webSocketRun(WebSocket *ws, WebSocketProc callback, void *arg, RBuf *buf, Ticks timeout)
{
    Ticks pingDue;
    ssize nbytes;

    ws->callback = callback;
    ws->callbackArg = arg;

    // Fire open event
    if (ws->state == WS_STATE_CONNECTING) {
        ws->state = WS_STATE_OPEN;
        invokeCallback(ws, WS_EVENT_OPEN, NULL, 0);
    }
    // Process any buffered data from HTTP upgrade
    if (buf && rGetBufLength(buf) > 0) {
        rPutBlockToBuf(ws->buf, rGetBufStart(buf), rGetBufLength(buf));
        rAdjustBufStart(ws->buf, (ssize) rGetBufLength(buf));
        webSocketProcess(ws);
    }
    pingDue = ws->pingPeriod ? rGetTicks() + ws->pingPeriod : 0;

    while (ws->state != WS_STATE_CLOSED && rState < R_STOPPING) {
        // Send ping if due
        if (pingDue && rGetTicks() >= pingDue) {
            webSocketSendBlock(ws, WS_MSG_PING, NULL, 0);
            pingDue = rGetTicks() + ws->pingPeriod;
        }
        ws->deadline = rGetTicks() + timeout;
        ws->deadline = pingDue ? min(ws->deadline, pingDue) : ws->deadline;

        // Read all available data
        while ((nbytes = readSocket(ws)) > 0) {
            webSocketProcess(ws);
            if (ws->state == WS_STATE_CLOSED) {
                break;
            }
        }
        if (nbytes < 0) {
            webSocketSendClose(ws, WS_STATUS_COMMS_ERROR, NULL);
            break;
        }
        if (ws->state == WS_STATE_CLOSED) {
            break;
        }
        if (rWaitForIO(ws->sock->wait, R_READABLE, ws->deadline) < 0) {
            if (rGetTicks() >= ws->deadline) {
                wsError(ws, 0, "Timeout waiting for WebSocket data");
                break;
            }
        }
    }
    return ws->error ? -ws->error : 0;
}

/*
    Process WebSocket frames and messages
    Return 0 on close, < 0 for error and 1 for message(s) received
 */
PUBLIC int webSocketProcess(WebSocket *ws)
{
    /*
        Process buffered data while we have complete frames
     */
    for (int rc = 1; rc > 0 && ws->state != WS_STATE_CLOSED; ) {
        switch (ws->frame) {
        default:
            return wsError(ws, 0, "Protocol error, unknown frame state");

        case WS_BEGIN:
            if ((rc = parseFrame(ws)) <= 0) {
                break;
            }
        //  Fall through
        case WS_MSG:
            if ((rc = parseMessage(ws)) <= 0) {
                break;
            }
            break;
        }
        if (ws->error) {
            if (ws->state != WS_STATE_CLOSED) {
                webSocketSendClose(ws, ws->error, NULL);
            }
            ws->state = WS_STATE_CLOSED;
            return -ws->error;
        }
    }
    return 1;
}

/*
    Parse an incoming WebSocket frame
    Return < 0 for error, 0 if not ready, 1 if frame parsed
 */
static int parseFrame(WebSocket *ws)
{
    RBuf   *buf;
    char   *fp;
    size_t len;
    int    i, fin, mask, lenBytes, opcode;

    buf = ws->buf;
    //  Must always have at least 2 bytes
    if (rGetBufLength(buf) < 2) {
        return 0;
    }
    fp = buf->start;

    if (GET_RSV(*fp) != 0) {
        return wsError(ws, 0, "Protocol error, bad reserved field");
    }
    fin = GET_FIN(*fp);
    opcode = GET_CODE(*fp);
    if (opcode == WS_MSG_CONT) {
        if (!ws->type) {
            return wsError(ws, 0, "Protocol error, continuation frame but not prior message");
        }
    } else if (opcode < WS_MSG_CONTROL && ws->type) {
        return wsError(ws, 0, "Protocol error, data frame received but expected a continuation frame");
    } else {
        ws->type = opcode;
    }
    if (opcode > WS_MSG_PONG) {
        return wsError(ws, 0, "Protocol error, bad frame opcode");
    }
    ws->opcode = opcode;
    ws->fin = fin;

    if (opcode >= WS_MSG_CONTROL && !fin) {
        return wsError(ws, 0, "Protocol error, fragmented control frame");
    }
    /*
        Extract the length and mask from the frame
     */
    fp++;
    len = GET_LEN(*fp);
    mask = GET_MASK(*fp);
    lenBytes = 1;
    if (len == 126) {
        lenBytes += 2;
        len = 0;
    } else if (len == 127) {
        lenBytes += 8;
        len = 0;
    }
    if (rGetBufLength(buf) < ((size_t) (lenBytes + 1 + (mask * 4)))) {
        // Return if we don't have the required packet control fields yet
        return 0;
    }
    fp++;
    while (--lenBytes > 0) {
        if (len > SSIZE_MAX / 8) {
            return wsError(ws, 0, "Protocol error, frame length too big");
        }
        len <<= 8;
        len += (uchar) * fp++;
    }
    if (len < 0) {
        return wsError(ws, 0, "Protocol error, bad frame length");
    }
    if (opcode >= WS_MSG_CONTROL && len > WS_MAX_CONTROL) {
        return wsError(ws, 0, "Protocol error, control frame too big");
    }
    if (len > ws->maxMessage) {
        return wsError(ws, 0, "Protocol error, message too big");
    }
    ws->frameLength = len;
    ws->frame = WS_MSG;
    ws->maskOffset = mask ? 0 : -1;
    if (mask) {
        for (i = 0; i < 4; i++) {
            ws->dataMask[i] = (uchar) * fp++;
        }
    }
    //  Consume the frame from the buffer
    rAdjustBufStart(buf, fp - rGetBufStart(buf));
    return 1;
}

/*
    Parse an incoming message
    Return < 0 for error, 0 if not ready, 1 if message parsed
 */
static int parseMessage(WebSocket *ws)
{
    RBuf   *buf;
    uchar  *cp, *end;
    char   *msg;
    size_t nbytes, len;
    int    event, validated;

    buf = ws->buf;

    //  Must have at least the frame length
    nbytes = rGetBufLength(buf);
    if (nbytes < ws->frameLength) {
        return 0;
    }
    if (ws->maskOffset >= 0) {
        if (ws->frameLength > rGetBufLength(buf)) {
            return wsError(ws, 0, "Frame length exceeds buffer size");
        }
        //  Unmask the message
        end = (uchar*) &buf->start[ws->frameLength];
        for (cp = (uchar*) buf->start; cp < end; cp++) {
            *cp = *cp ^ ws->dataMask[ws->maskOffset++ & 0x3];
        }
    }
    /*
        Process the message based on the opcode
     */
    switch (ws->opcode) {
    case WS_MSG_TEXT:
    case WS_MSG_BINARY:
        ws->messageLength = 0;

    // Fall through
    case WS_MSG_CONT:
        if (ws->closing) {
            break;
        }
        if (ws->validate) {
            // Validate this frame if we don't have a partial codepoint from a prior frame
            validated = 0;
            if (ws->type == WS_MSG_TEXT && !ws->partialUTF) {
                if (!validateText(ws)) {
                    return wsError(ws, WS_STATUS_INVALID_UTF8, "Text packet has invalid UTF8");
                }
                validated++;
            }
            if (ws->type == WS_MSG_TEXT && !validated) {
                if (!validateText(ws)) {
                    return wsError(ws, WS_STATUS_INVALID_UTF8, "Text packet has invalid UTF8");
                }
            }
        }
        //  Update total message length over all frames
        if (ws->messageLength > SSIZE_MAX - rGetBufLength(buf)) {
            return wsError(ws, 0, "Protocol error, message length too big");
        }
        ws->messageLength += rGetBufLength(buf);

        if (ws->callback) {
            event = ws->fin ? WS_EVENT_MESSAGE : WS_EVENT_PARTIAL_MESSAGE;
            if (rGetBufLength(buf) > ws->frameLength) {
                /*
                    Multiple messages in the buffer, but very valuable to guarantee that the
                    message is null terminated (at a performance cost)
                 */
                msg = snclone(rGetBufStart(buf), ws->frameLength);
                invokeCallback(ws, event, msg, ws->frameLength);
                rFree(msg);
            } else {
                rAddNullToBuf(buf);
                invokeCallback(ws, event, rGetBufStart(buf), ws->frameLength);
            }
        }
        //  Consume the data from the buffer
        rAdjustBufStart(buf, (ssize) ws->frameLength);
        if (ws->fin) {
            ws->frame = WS_BEGIN;
        }
        break;

    case WS_MSG_CLOSE:
        cp = (uchar*) buf->start;
        if (rGetBufLength(buf) == 0) {
            ws->closeStatus = WS_STATUS_OK;
        } else if (rGetBufLength(buf) < 2) {
            return wsError(ws, 0, "Missing close status");
        } else {
            /*
                WebSockets is a hideous spec, as if UTF validation wasn't bad enough, we must invalidate these codes:
                    1004, 1005, 1006, 1012-1016, 2000-2999
             */
            ws->closeStatus = (cp[0] << 8) | cp[1];
            if (ws->closeStatus < 1000 || ws->closeStatus >= 5000 ||
                (1004 <= ws->closeStatus && ws->closeStatus <= 1006) ||
                (1012 <= ws->closeStatus && ws->closeStatus <= 1016) ||
                (1200 <= ws->closeStatus && ws->closeStatus <= 2999)) {
                return wsError(ws, 0, "Bad close status %d", ws->closeStatus);
            }
            rAdjustBufStart(buf, 2);
            if (rGetBufLength(buf) > 0) {
                rFree(ws->closeReason);
                ws->closeReason = sclone(buf->start);
                if (ws->validate) {
                    if (validUTF8(ws, ws->closeReason, slen(ws->closeReason)) != UTF8_ACCEPT) {
                        return wsError(ws, WS_STATUS_INVALID_UTF8, "Text packet has invalid UTF8");
                    }
                }
            }
        }
        if (!ws->closing) {
            // Acknowledge the close. Echo the received status
            webSocketSendClose(ws, WS_STATUS_OK, "OK");
        }
        ws->state = WS_STATE_CLOSED;
        break;

    case WS_MSG_PING:
        //  Respond with the same content as specified in the ping message
        len = min(rGetBufLength(buf), WS_MAX_CONTROL);
        if (webSocketSendBlock(ws, WS_MSG_PONG, rGetBufStart(buf), len) < 0) {
            // Error set
        }
        //  Consume message from the buffer
        rAdjustBufStart(buf, (ssize) ws->frameLength);
        rCompactBuf(buf);
        ws->frame = WS_BEGIN;
        break;

    case WS_MSG_PONG:
        ws->frame = WS_BEGIN;
        break;

    default:
        return wsError(ws, 0, "Bad message type %d", ws->type);
    }
    if (ws->frame == WS_BEGIN) {
        ws->frameLength = 0;
        ws->messageLength = 0;
        ws->type = 0;
    }
    return 1;
}

/*
    Send a text message. Caller must submit valid UTF8.
    Returns the number of data message bytes written. Should equal the length.
 */
PUBLIC ssize webSocketSend(WebSocket *ws, cchar *fmt, ...)
{
    va_list args;
    char    *buf;
    ssize   bytes;

    va_start(args, fmt);
    buf = sfmtv(fmt, args);
    va_end(args);
    bytes = webSocketSendBlock(ws, WS_MSG_TEXT, buf, slen(buf));
    rFree(buf);
    return bytes;
}

/*
    Write all or a portion of a json object
 */
PUBLIC ssize webSocketSendJson(WebSocket *ws, Json *json, int nid, cchar *key)
{
    char  *str;
    ssize rc;

    str = jsonToString(json, nid, key, JSON_JSON);
    rc = webSocketSendString(ws, str);
    rFree(str);
    return rc;
}

PUBLIC ssize webSocketSendString(WebSocket *ws, cchar *buf)
{
    return webSocketSendBlock(ws, WS_MSG_TEXT, buf, slen(buf));
}

/*
    Send a block of data with the specified message type
 */
PUBLIC ssize webSocketSendBlock(WebSocket *ws, int type, cchar *buf, size_t len)
{
    size_t thisWrite, totalWritten;
    int    fin, more;

    assert(ws);

    more = (type & WS_MSG_MORE) ? 1 : 0;
    type &= ~WS_MSG_MORE;

    if (type != WS_MSG_CONT && type != WS_MSG_TEXT && type != WS_MSG_BINARY && type != WS_MSG_CLOSE &&
        type != WS_MSG_PING && type != WS_MSG_PONG) {
        return wsError(ws, 0, "Bad message type %d", type);
    }
    if (len > ws->maxMessage) {
        return wsError(ws, R_ERR_WONT_FIT, "Outgoing message is too large, length %zd max %zd", len, ws->maxMessage);
    }
    totalWritten = 0;
    do {
        thisWrite = min(len, (size_t) ws->maxFrame);

        fin = ((len - thisWrite) > 0) ? 0 : !more;
        if (writeFrame(ws, type, fin, (cuchar*) buf, thisWrite) < 0) {
            // Error set
            break;
        }
        len -= thisWrite;
        buf += thisWrite;
        totalWritten += thisWrite;
        if (more) {
            //  Spec requires type to be set only on the first frame
            type = 0;
        }
    } while (len > 0);

    return (ssize) totalWritten;
}

static int writeFrame(WebSocket *ws, int type, int fin, cuchar *buf, size_t len)
{
    uchar *pp, prefix[16];
    uchar *op, dataMask[4], *tbuf;
    int   i, mask;

    if (type < 0 || type > WS_MSG_MAX) {
        wsError(ws, 0, "Bad WebSocket packet type %d", type);
        return R_ERR_BAD_STATE;
    }
    /*
        Server-side does not mask outgoing data
     */
    mask = ws->client ? 1 : 0;
    pp = prefix;
    *pp++ = SET_FIN(fin) | SET_CODE(type);
    if (len <= WS_MAX_CONTROL) {
        *pp++ = SET_MASK(mask) | SET_LEN(len, 0);
    } else if (len <= 65535) {
        *pp++ = SET_MASK(mask) | 126;
        *pp++ = SET_LEN(len, 1);
        *pp++ = SET_LEN(len, 0);
    } else {
        *pp++ = SET_MASK(mask) | 127;
        for (i = 7; i >= 0; i--) {
            *pp++ = SET_LEN(len, i);
        }
    }
    tbuf = 0;
    if (ws->client) {
        cryptGetRandomBytes((uchar*) dataMask, sizeof(dataMask), 1);
        for (i = 0; i < 4; i++) {
            *pp++ = dataMask[i];
        }
        if ((tbuf = rAlloc(len)) == 0) {
            wsError(ws, 0, "Cannot allocate memory");
            return R_ERR_MEMORY;
        }
        for (i = 0, op = (uchar*) tbuf; op < (uchar*) &tbuf[len]; op++, i++) {
            *op = buf[i] ^ dataMask[i & 0x3];
        }
        buf = tbuf;
    }
    *pp = '\0';
    if (rWriteSocket(ws->sock, prefix, (size_t) (pp - prefix), ws->deadline) < 0) {
        if (type != WS_MSG_CLOSE) {
            wsError(ws, 0, "Cannot write to socket");
        }
        rFree(tbuf);
        return R_ERR_CANT_WRITE;
    }
    if (rWriteSocket(ws->sock, buf, len, ws->deadline) < 0) {
        if (type != WS_MSG_CLOSE) {
            wsError(ws, 0, "Cannot write to socket");
        }
        rFree(tbuf);
        return R_ERR_CANT_WRITE;
    }
    rFree(tbuf);
    return 0;
}

/*
    The reason string is optional
 */
PUBLIC ssize webSocketSendClose(WebSocket *ws, int status, cchar *reason)
{
    char   msg[128];
    size_t len;

    assert(0 <= status && status <= WS_STATUS_MAX);
    if (ws->closing || ws->state == WS_STATE_CLOSED) {
        return 0;
    }
    ws->closing = 1;
    ws->state = WS_STATE_CLOSING;

    len = 2;
    if (reason) {
        if (slen(reason) >= 124) {
            reason = "WebSockets close message was too big";
            wsError(ws, R_ERR_WONT_FIT, reason);
        }
        len += slen(reason) + 1;
    }
    msg[0] = (status >> 8) & 0xff;
    msg[1] = status & 0xff;
    if (reason) {
        scopy(&msg[2], len - 2, reason);
    }
    invokeCallback(ws, WS_EVENT_CLOSE, msg, len);
    return webSocketSendBlock(ws, WS_MSG_CLOSE, msg, len);
}

PUBLIC cchar *webSocketGetCloseReason(WebSocket *ws)
{
    if (!ws) {
        return 0;
    }
    return ws->closeReason;
}

PUBLIC cchar *webSocketGetClientKey(WebSocket *ws)
{
    return ws->clientKey;
}

PUBLIC void *webSocketGetData(WebSocket *ws)
{
    return ws->data;
}

PUBLIC cchar *webSocketGetErrorMessage(WebSocket *ws)
{
    if (!ws) {
        return 0;
    }
    return ws->errorMessage;
}

PUBLIC ssize webSocketGetMessageLength(WebSocket *ws)
{
    if (!ws) {
        return 0;
    }
    return (ssize) ws->messageLength;
}

PUBLIC char *webSocketGetProtocol(WebSocket *ws)
{
    if (!ws) {
        return 0;
    }
    return ws->protocol;
}

PUBLIC ssize webSocketGetState(WebSocket *ws)
{
    if (!ws) {
        return 0;
    }
    return ws->state;
}

PUBLIC bool webSocketGetOrderlyClosed(WebSocket *ws)
{
    return ws->closeStatus != WS_STATUS_COMMS_ERROR;
}

PUBLIC void webSocketSetClientKey(WebSocket *ws, cchar *clientKey)
{
    rFree(ws->clientKey);
    ws->clientKey = sclone(clientKey);
}

PUBLIC void webSocketSetData(WebSocket *ws, void *data)
{
    ws->data = data;
}

PUBLIC void webSocketSetLimits(WebSocket *ws, size_t maxFrame, size_t maxMessage)
{
    ws->maxFrame = maxFrame;
    ws->maxMessage = maxMessage;
}

PUBLIC void webSocketSetPingPeriod(WebSocket *ws, Time pingPeriod)
{
    ws->pingPeriod = pingPeriod;
}

/*
    Save the selected WebSocket protocol. This is the application-level protocol negotiated with the server.
 */
PUBLIC void webSocketSelectProtocol(WebSocket *ws, cchar *protocol)
{
    if (!protocol || !*protocol) {
        return;
    }
    rFree(ws->protocol);
    ws->protocol = sclone(protocol);
}

PUBLIC void webSocketSetValidateUTF(WebSocket *ws, bool validateUTF)
{
    ws->validate = validateUTF;
}

/*
    Test if a string is a valid unicode string. The return state may be UTF8_ACCEPT if
    all codepoints validate and are complete. Return UTF8_REJECT if an invalid codepoint was found.
    Otherwise, return the state for a partial codepoint.
 */
static uint validUTF8(WebSocket *ws, cchar *str, size_t len)
{
    uchar *cp, c;
    uint  state, type;

    state = UTF8_ACCEPT;
    for (cp = (uchar*) str; cp < (uchar*) &str[len]; cp++) {
        /*
            codepoint = (*state != UTF8_ACCEPT) ? (byte & 0x3fu) | (*codep << 6) : (0xff >> type) & (byte);
         */
        c = *cp;
        type = utfTable[c];
        state = utfTable[256 + (state * 16) + type];
        if (state == UTF8_REJECT) {
            wsError(ws, WS_STATUS_INVALID_UTF8, "Invalid UTF8 at offset %d", cp - (uchar*) str);
            break;
        }
    }
    return state;
}

/*
    Validate the UTF8 in a packet. Return false if an invalid codepoint is found.
    If the packet is not the last packet, we alloc incomplete codepoints.
    Set ws->partialUTF if the last codepoint was incomplete.
 */
static bool validateText(WebSocket *ws)
{
    uint state;
    bool valid;

    /*
        Skip validation if ignoring errors or some frames have already been sent to the callback
     */
    if (!ws->validate || ws->messageLength > 0) {
        return 1;
    }
    state = validUTF8(ws, ws->buf->start, (size_t) ws->frameLength);
    ws->partialUTF = state != UTF8_ACCEPT;

    if (ws->fin) {
        valid = state == UTF8_ACCEPT;
    } else {
        valid = state != UTF8_REJECT;
    }
    return valid;
}

/*
    Set an error message and return a status code.
    The code can be either a WebSocket status code or a safe runtime status code (< 0).
    If WebSocket status code, ws->error is set.
 */
static int wsError(WebSocket *ws, int code, cchar *fmt, ...)
{
    va_list args;

    if (code == 0) {
        code = WS_STATUS_PROTOCOL_ERROR;
    }
    ws->error = code;

    va_start(args, fmt);
    ws->errorMessage = sfmtv(fmt, args);
    va_end(args);

    rTrace("sockets", "%s", ws->errorMessage);
    invokeCallback(ws, WS_EVENT_ERROR, ws->errorMessage, slen(ws->errorMessage));
    return -code;
}

#endif /* ME_COM_WEBSOCK */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */

#else
void dummyWebSock(){}
#endif /* ME_COM_WEBSOCK */
