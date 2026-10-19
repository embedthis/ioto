/*
    websockLib.c -- WebSocket Library Source

    This file is a catenation of all the source code. Amalgamating into a
    single file makes embedding simpler and the resulting application faster,
    by using compiler optimization within the library.

    Prepared by: buildLib.sh
 */

#include "websock.h"

#if ME_COM_WEBSOCK


/********* Start of file src/websockLib.c ************/

/*
    websockLib.c - Web Sockets

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "websock.h"
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

#define UTF8_ACCEPT     0
#define UTF8_REJECT     1

/*
    The largest frame the receive buffer can hold. A whole frame is buffered before it is delivered,
    and parseMessage adds a null after it so the callback gets a terminated payload, so the frame
    must leave room for that byte. Accepting a maxFrame above this used to desynchronise the stream:
    the buffer could not grow for the null, the payload was never fully consumed, and the parser read
    it as the next frame header.
 */
#define WS_MAX_BUFFERED (ME_R_MAX_BUF - 1)

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

static void abandonConnection(WebSocket *ws);
static void invokeCallback(WebSocket *ws, int event, cchar *buf, size_t len);
static int parseMessage(WebSocket *ws);
static int parseFrame(WebSocket *ws);
static uint validUTF8(uint state, cchar *str, size_t len);
static bool validateText(WebSocket *ws);
static int writeFrame(WebSocket *ws, int type, int fin, cuchar *buf, size_t len);
static int wsError(WebSocket *ws, int code, cchar *fmt, ...);
static int wsResult(WebSocket *ws);

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
    webSocketSetLimits(ws, WS_MAX_FRAME, WS_MAX_MESSAGE);
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
    Read data from the socket into the WebSocket buffer, waiting until the deadline for some.
    Return the number of bytes read, R_ERR_TIMEOUT if the deadline passed with none, < 0 on error.
 */
static ssize readSocket(WebSocket *ws, Ticks deadline)
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

    if ((nbytes = rReadSocket(ws->sock, bp->end, toRead, deadline)) < 0) {
        return nbytes;
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
    Ticks pingDue, readUntil, now;
    ssize nbytes;

    if (!ws) {
        return R_ERR_BAD_ARGS;
    }
    ws->callback = callback;
    ws->callbackArg = arg;

    // Fire open event
    if (ws->state == WS_STATE_CONNECTING) {
        ws->state = WS_STATE_OPEN;
        invokeCallback(ws, WS_EVENT_OPEN, NULL, 0);
    }
    // Process any data the caller read ahead of the WebSocket stream, such as after the HTTP upgrade
    if (buf && rGetBufLength(buf) > 0) {
        rPutBlockToBuf(ws->buf, rGetBufStart(buf), rGetBufLength(buf));
        rAdjustBufStart(buf, (ssize) rGetBufLength(buf));
        webSocketProcess(ws);
    }
    pingDue = ws->pingPeriod ? rGetTicks() + ws->pingPeriod : 0;

    while (ws->state != WS_STATE_CLOSED && rState < R_STOPPING) {
        now = rGetTicks();
        if (pingDue && now >= pingDue) {
            webSocketSendBlock(ws, WS_MSG_PING, NULL, 0);
            pingDue = now + ws->pingPeriod;
        }
        /*
            Inactivity bounds reads and writes; the ping deadline only wakes idle reads to send a ping. Zero
            disables inactivity expiry.
         */
        ws->deadline = timeout > 0 ? now + timeout : MAXINT64;
        readUntil = pingDue ? min(ws->deadline, pingDue) : ws->deadline;

        if ((nbytes = readSocket(ws, readUntil)) > 0) {
            webSocketProcess(ws);
        } else if (nbytes == R_ERR_TIMEOUT) {
            if (rGetTicks() >= ws->deadline) {
                wsError(ws, 0, "Timeout waiting for WebSocket data");
                abandonConnection(ws);
                break;
            }
        } else {
            wsError(ws, 0, "Cannot read from socket");
            abandonConnection(ws);
            break;
        }
    }
    return wsResult(ws);
}

/*
    Give up on the peer. The abnormal close is recorded locally as 1006, which is what that code is
    for, and the peer is told we are going away, which is what may be sent on the wire.
 */
static void abandonConnection(WebSocket *ws)
{
    ws->closeStatus = WS_STATUS_COMMS_ERROR;
    webSocketSendClose(ws, WS_STATUS_GOING_AWAY, NULL);
}

/*
    Process WebSocket frames and messages
    Return 0 on close, < 0 for error and 1 for message(s) received
 */
PUBLIC int webSocketProcess(WebSocket *ws)
{
    if (!ws) {
        return R_ERR_BAD_ARGS;
    }
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
                //  A runtime error has no WebSocket status of its own
                webSocketSendClose(ws, ws->error > 0 ? ws->error : WS_STATUS_INTERNAL_ERROR, NULL);
            }
            ws->state = WS_STATE_CLOSED;
            return wsResult(ws);
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
    /*
        Only continuation, text, binary, close, ping and pong are assigned. The reserved data
        opcodes 3-7 are refused here with every other header check, so the frame is refused before
        its payload is buffered rather than after.
     */
    if (opcode > WS_MSG_PONG || (WS_MSG_BINARY < opcode && opcode < WS_MSG_CONTROL)) {
        return wsError(ws, 0, "Protocol error, bad frame opcode");
    }
    if (opcode == WS_MSG_CONT) {
        if (!ws->type) {
            return wsError(ws, 0, "Protocol error, continuation frame but not prior message");
        }
    } else if (opcode < WS_MSG_CONTROL && ws->type) {
        return wsError(ws, 0, "Protocol error, data frame received but expected a continuation frame");
    }
    if (opcode >= WS_MSG_CONTROL && !fin) {
        return wsError(ws, 0, "Protocol error, fragmented control frame");
    }
    /*
        Extract the length and mask from the frame. Nothing is committed to the WebSocket until the
        whole header is present: a header split across two reads is parsed again from the start,
        and a type committed on the first pass made the second pass refuse it as a stray data frame.
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
    /*
        A client masks every frame and a server masks none (RFC 6455 section 5.1). The mask defeats
        cache poisoning through intermediaries, so the wrong choice fails the connection.
     */
    if (mask == ws->client) {
        return wsError(ws, 0, ws->client ? "Protocol error, masked server frame" :
                       "Protocol error, unmasked client frame");
    }
    fp++;
    while (--lenBytes > 0) {
        len <<= 8;
        len += (uchar) * fp++;
    }
    if (opcode >= WS_MSG_CONTROL && len > WS_MAX_CONTROL) {
        return wsError(ws, 0, "Protocol error, control frame too big");
    }
    /*
        Bound the frame and the assembled message separately.

        A message may be fragmented across any number of continuation frames, so checking only the
        frame length bounds nothing: an attacker sends frames each just under the limit and never
        sets FIN, and the assembled message grows without limit. maxFrame was previously applied
        only to outgoing writes.

        The check is made here, from the declared length in the frame header, so the data is
        refused before it is buffered rather than after.
     */
    if (opcode < WS_MSG_CONTROL) {
        if ((size_t) len > ws->maxFrame) {
            return wsError(ws, WS_STATUS_MESSAGE_TOO_LARGE, "Protocol error, frame too big");
        }
        if (ws->messageLength + (size_t) len > ws->maxMessage) {
            return wsError(ws, WS_STATUS_MESSAGE_TOO_LARGE, "Protocol error, message too big");
        }
    }
    //  The message type belongs to the message. A control frame interleaved in it must not touch it.
    if (opcode == WS_MSG_TEXT || opcode == WS_MSG_BINARY) {
        ws->type = opcode;
    }
    ws->opcode = opcode;
    ws->fin = fin;
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
    size_t nbytes, len, reasonLen;
    int    event;

    buf = ws->buf;

    //  Must have at least the frame length
    nbytes = rGetBufLength(buf);
    if (nbytes < ws->frameLength) {
        return 0;
    }
    if (ws->maskOffset >= 0) {
        //  Unmask the message. The whole frame is buffered, guaranteed by the length check above.
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
        ws->utfState = UTF8_ACCEPT;

    // Fall through
    case WS_MSG_CONT:
        if (ws->closing) {
            //  Discard data received after our close was sent. Leaving it buffered spun forever.
            rAdjustBufStart(buf, (ssize) ws->frameLength);
            ws->frame = WS_BEGIN;
            break;
        }
        if (ws->validate && ws->type == WS_MSG_TEXT && !validateText(ws)) {
            return wsError(ws, WS_STATUS_INVALID_UTF8, "Text packet has invalid UTF8");
        }
        /*
            Update total message length over all frames. Count this frame, not the buffer - the
            buffer may hold following pipelined frames, which would over-count the message and
            refuse a message that is within the limit.
         */
        if (ws->messageLength > SSIZE_MAX - ws->frameLength) {
            return wsError(ws, 0, "Protocol error, message length too big");
        }
        ws->messageLength += ws->frameLength;
        if (ws->messageLength > ws->maxMessage) {
            //  Backstop - the header check above should have caught this
            return wsError(ws, WS_STATUS_MESSAGE_TOO_LARGE, "Protocol error, message too big");
        }

        if (ws->callback) {
            event = ws->fin ? WS_EVENT_MESSAGE : WS_EVENT_PARTIAL_MESSAGE;
            if (rGetBufLength(buf) > ws->frameLength) {
                /*
                    Copy the full payload, including embedded NULs, and terminate it for callbacks.
                 */
                msg = rAlloc(ws->frameLength + 1);
                memcpy(msg, buf->start, ws->frameLength);
                msg[ws->frameLength] = '\0';
                invokeCallback(ws, event, msg, ws->frameLength);
                rFree(msg);
            } else {
                rAddNullToBuf(buf);
                invokeCallback(ws, event, rGetBufStart(buf), ws->frameLength);
            }
        }
        //  Consume the data from the buffer
        rAdjustBufStart(buf, (ssize) ws->frameLength);
        /*
            The next bytes on the wire are always a new frame header, final frame or not. Staying
            in WS_MSG consumed the following frame's header as if it were more payload, so a
            fragmented message was mis-parsed and the connection then stalled waiting for payload
            that never arrived.
         */
        ws->frame = WS_BEGIN;
        break;

    case WS_MSG_CLOSE:
        cp = (uchar*) buf->start;
        // Use this frame's length; the buffer may contain another frame.
        if (ws->frameLength == 0) {
            ws->closeStatus = WS_STATUS_OK;
        } else if (ws->frameLength < 2) {
            return wsError(ws, 0, "Missing close status");
        } else {
            /*
                RFC 6455 section 7.4 assigns 1000-1003 and 1007-1011 for the protocol, 3000-3999 to
                libraries and 4000-4999 to applications. Everything else is unassigned or reserved
                for local reporting (1004-1006, 1015) and must not appear on the wire. Listing the
                codes that are allowed rather than the ranges that are not is what closes the hole:
                the previous set of exclusions left 1017-1199 accepted.
             */
            ws->closeStatus = (cp[0] << 8) | cp[1];
            if (!((1000 <= ws->closeStatus && ws->closeStatus <= 1003) ||
                  (1007 <= ws->closeStatus && ws->closeStatus <= 1011) ||
                  (3000 <= ws->closeStatus && ws->closeStatus <= 4999))) {
                return wsError(ws, 0, "Bad close status %d", ws->closeStatus);
            }
            rAdjustBufStart(buf, 2);
            if (ws->frameLength > 2) {
                rFree(ws->closeReason);
                /*
                    Copy only this frame's reason bytes, excluding the status.
                 */
                reasonLen = ws->frameLength - 2;
                ws->closeReason = rAlloc(reasonLen + 1);
                memcpy(ws->closeReason, buf->start, reasonLen);
                ws->closeReason[reasonLen] = '\0';
                if (ws->validate) {
                    if (validUTF8(UTF8_ACCEPT, ws->closeReason, slen(ws->closeReason)) != UTF8_ACCEPT) {
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
        //  Respond with the ping payload only. The buffer may hold following frames.
        //  A write failure records the error itself and is reported when this call returns.
        len = ws->frameLength;
        webSocketSendBlock(ws, WS_MSG_PONG, rGetBufStart(buf), len);
        //  Consume message from the buffer
        rAdjustBufStart(buf, (ssize) ws->frameLength);
        rCompactBuf(buf);
        ws->frame = WS_BEGIN;
        break;

    case WS_MSG_PONG:
        //  Consume the payload, which is otherwise parsed as the next frame header
        rAdjustBufStart(buf, (ssize) ws->frameLength);
        ws->frame = WS_BEGIN;
        break;

    default:
        //  Backstop - parseFrame refuses every unassigned opcode
        return wsError(ws, 0, "Bad message type %d", ws->opcode);
    }
    if (ws->frame == WS_BEGIN) {
        ws->frameLength = 0;
        /*
            messageLength and type belong to the message, not the frame, so they survive a
            continuation frame and an interleaved control frame. Resetting them per frame is what
            let a fragmented message grow past maxMessage.
         */
        if (ws->opcode < WS_MSG_CONTROL && ws->fin) {
            ws->messageLength = 0;
            ws->type = 0;
        }
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

    if (!ws) {
        return R_ERR_BAD_ARGS;
    }
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

    if (!ws) {
        return R_ERR_BAD_ARGS;
    }
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

    if (!ws) {
        return R_ERR_BAD_ARGS;
    }
    more = (type & WS_MSG_MORE) ? 1 : 0;
    type &= ~WS_MSG_MORE;

    if (type != WS_MSG_CONT && type != WS_MSG_TEXT && type != WS_MSG_BINARY && type != WS_MSG_CLOSE &&
        type != WS_MSG_PING && type != WS_MSG_PONG) {
        return wsError(ws, 0, "Bad message type %d", type);
    }
    if (len > ws->maxMessage) {
        return wsError(ws, R_ERR_WONT_FIT, "Outgoing message is too large, length %zd max %zd", len, ws->maxMessage);
    }
    /*
        A control frame is never fragmented, so an oversize payload cannot be split and would go on
        the wire as a control frame with an extended length, which the peer must reject.
     */
    if (type >= WS_MSG_CONTROL && len > WS_MAX_CONTROL) {
        return wsError(ws, R_ERR_WONT_FIT, "Outgoing control frame is too large, length %zd max %d", len,
                       WS_MAX_CONTROL);
    }
    totalWritten = 0;
    do {
        //  Control frames are never fragmented. They are bounded by WS_MAX_CONTROL.
        thisWrite = (type >= WS_MSG_CONTROL) ? len : min(len, (size_t) ws->maxFrame);

        fin = ((len - thisWrite) > 0) ? 0 : !more;
        if (writeFrame(ws, type, fin, (cuchar*) buf, thisWrite) < 0) {
            // Error set
            break;
        }
        len -= thisWrite;
        buf += thisWrite;
        totalWritten += thisWrite;
        //  Every frame after the first is a continuation, whether the caller or maxFrame split it
        type = WS_MSG_CONT;
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
        if (cryptGetRandomBytes(dataMask, sizeof(dataMask), 1) < 0) {
            wsError(ws, WS_STATUS_INTERNAL_ERROR, "Cannot get random bytes for the frame mask");
            return R_ERR_CANT_COMPLETE;
        }
        for (i = 0; i < 4; i++) {
            *pp++ = dataMask[i];
        }
        tbuf = rAlloc(len);
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

    if (!ws) {
        return R_ERR_BAD_ARGS;
    }
    assert(0 <= status && status <= WS_STATUS_MAX);
    if (ws->closing || ws->state == WS_STATE_CLOSED) {
        return 0;
    }
    ws->closing = 1;
    ws->state = WS_STATE_CLOSING;

    /*
        RFC 6455 reserves these codes for local reporting. A peer must refuse them on the wire, and
        this parser does, so they are sent as an internal error.
     */
    if (status == WS_STATUS_FRAME_TOO_LARGE || status == WS_STATUS_NO_STATUS || status == WS_STATUS_COMMS_ERROR ||
        status == WS_STATUS_TLS_ERROR) {
        status = WS_STATUS_INTERNAL_ERROR;
    }

    len = 2;
    if (reason) {
        if (slen(reason) >= 124) {
            reason = "WebSockets close message was too big";
            wsError(ws, R_ERR_WONT_FIT, reason);
        }
        len += slen(reason);
    }
    msg[0] = (status >> 8) & 0xff;
    msg[1] = status & 0xff;
    if (reason) {
        //  The payload is the reason text. A terminator on the wire is a stray byte to the peer.
        memcpy(&msg[2], reason, len - 2);
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
    if (!ws) {
        return 0;
    }
    return ws->clientKey;
}

PUBLIC void *webSocketGetData(WebSocket *ws)
{
    if (!ws) {
        return 0;
    }
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
    if (!ws) {
        return 0;
    }
    return ws->closeStatus != WS_STATUS_COMMS_ERROR;
}

PUBLIC void webSocketSetClientKey(WebSocket *ws, cchar *clientKey)
{
    if (!ws) {
        return;
    }
    rFree(ws->clientKey);
    ws->clientKey = sclone(clientKey);
}

PUBLIC void webSocketSetData(WebSocket *ws, void *data)
{
    if (!ws) {
        return;
    }
    ws->data = data;
}

PUBLIC void webSocketSetLimits(WebSocket *ws, size_t maxFrame, size_t maxMessage)
{
    if (!ws) {
        return;
    }
    /*
        Zero means no limit, which is what the header has always documented. A zero maxFrame was
        taken literally by the send loop, where thisWrite became zero, no progress was made and
        empty continuation frames went out without end. A frame is bounded in any case by what a
        buffer can hold. A message is delivered a frame at a time and never buffered whole, so
        no limit there is genuinely no limit.
     */
    if (maxFrame == 0) {
        maxFrame = WS_MAX_BUFFERED;
    } else if (maxFrame > WS_MAX_BUFFERED) {
        rTrace("sockets", "maxFrame %zd exceeds the largest buffered frame, using %d", maxFrame, WS_MAX_BUFFERED);
        maxFrame = WS_MAX_BUFFERED;
    }
    if (maxMessage == 0) {
        maxMessage = SSIZE_MAX;
    }
    ws->maxFrame = maxFrame;
    ws->maxMessage = maxMessage;
}

PUBLIC void webSocketSetPingPeriod(WebSocket *ws, Time pingPeriod)
{
    if (!ws) {
        return;
    }
    ws->pingPeriod = pingPeriod;
}

/*
    Save the selected WebSocket protocol. This is the application-level protocol negotiated with the server.
 */
PUBLIC void webSocketSelectProtocol(WebSocket *ws, cchar *protocol)
{
    if (!ws || !protocol || !*protocol) {
        return;
    }
    rFree(ws->protocol);
    ws->protocol = sclone(protocol);
}

PUBLIC void webSocketSetValidateUTF(WebSocket *ws, bool validateUTF)
{
    if (!ws) {
        return;
    }
    ws->validate = validateUTF;
}

/*
    Test if a string is a valid unicode string, resuming from the given decoder state.
    The return state may be UTF8_ACCEPT if all codepoints validate and are complete.
    Return UTF8_REJECT if an invalid codepoint was found.
    Otherwise, return the state for a partial codepoint.
 */
static uint validUTF8(uint state, cchar *str, size_t len)
{
    uchar *cp, c;
    uint  type;

    for (cp = (uchar*) str; cp < (uchar*) &str[len]; cp++) {
        /*
            codepoint = (*state != UTF8_ACCEPT) ? (byte & 0x3fu) | (*codep << 6) : (0xff >> type) & (byte);
         */
        c = *cp;
        type = utfTable[c];
        state = utfTable[256 + (state * 16) + type];
        if (state == UTF8_REJECT) {
            break;
        }
    }
    return state;
}

/*
    Validate the UTF8 in this frame. Return false if an invalid codepoint is found or if the message
    ends on an incomplete one.
 */
static bool validateText(WebSocket *ws)
{
    /*
        The decoder state carries across the frames of a message, so a codepoint split over a frame
        boundary validates and an invalid byte is caught in any frame. Validating only the first
        frame of a message accepted whatever every continuation frame that followed happened to hold.
     */
    ws->utfState = validUTF8(ws->utfState, ws->buf->start, ws->frameLength);
    if (ws->utfState == UTF8_REJECT) {
        return 0;
    }
    //  A message may only end on a complete codepoint
    return ws->fin ? ws->utfState == UTF8_ACCEPT : 1;
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
    rFree(ws->errorMessage);
    ws->errorMessage = sfmtv(fmt, args);
    va_end(args);

    rTrace("sockets", "%s", ws->errorMessage);
    invokeCallback(ws, WS_EVENT_ERROR, ws->errorMessage, slen(ws->errorMessage));
    return wsResult(ws);
}

/*
    The negative result for the current error. Runtime codes are already negative, WebSocket status
    codes are positive and are negated. Zero if there is no error.
 */
static int wsResult(WebSocket *ws)
{
    return ws->error < 0 ? ws->error : -ws->error;
}

#endif /* ME_COM_WEBSOCK */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */

#else
void dummyWebSock(){}
#endif /* ME_COM_WEBSOCK */
