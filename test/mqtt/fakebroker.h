/*
    fakebroker.h - Scripted MQTT broker for protocol-level unit tests

    Listens on an ephemeral loopback port, records every packet the client sends and answers as a
    broker would unless the test says otherwise. Tests can also inject raw bytes to drive the
    client's packet parser through its error paths. All I/O runs on fibers, so a test fiber that
    yields (rSleep, WAIT_UNTIL, MQTT_WAIT_ACK) lets both the broker and the client make progress.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

#ifndef _h_FAKE_BROKER
#define _h_FAKE_BROKER 1

/********************************** Includes **********************************/

#include    "mqtt-test.h"

/*********************************** Types ************************************/

typedef struct FakePacket {
    uchar *data;            /* Whole packet including the fixed header */
    size_t len;
    int type;               /* MqttPacketType */
    int flags;              /* Fixed header flags */
    int id;                 /* Packet identifier, zero if the packet has none */
} FakePacket;

typedef struct FakeBroker {
    RSocket *listen;
    RSocket *client;        /* Accepted connection, NULL when none */
    RList *packets;         /* FakePacket* in arrival order */
    RBuf *in;
    int port;
    int connackCode;        /* CONNACK return code */
    int subackCode;         /* SUBACK return code */
    int ackConnect;         /* Answer CONNECT (default on) */
    int ackPublish;         /* Answer QoS 1 and 2 PUBLISH, PUBREC and PUBREL (default on) */
    int ackSubscribe;       /* Answer SUBSCRIBE and UNSUBSCRIBE (default on) */
    int ackPing;            /* Answer PINGREQ (default on) */
    int accepted;           /* Connections accepted so far */
    int closing;            /* Ask the connection handler to drop the client */
    int running;            /* Connection handler is active */
} FakeBroker;

/************************************ Code ************************************/

/*
    Encode an MQTT remaining length. Returns the number of bytes written.
 */
static inline size_t fbEncodeLength(uchar *bp, size_t length)
{
    size_t count;

    count = 0;
    do {
        bp[count] = (uchar) (length & 0x7F);
        length >>= 7;
        if (length > 0) {
            bp[count] |= 0x80;
        }
        count++;
    } while (length > 0);
    return count;
}

/*
    Size of the complete packet at the start of the buffer, or zero if more bytes are needed
 */
static inline size_t fbPacketLength(cuchar *bp, size_t avail)
{
    size_t length, pos;
    int    shift;

    length = 0;
    shift = 0;
    pos = 1;
    do {
        if (pos >= avail || shift > 21) {
            return 0;
        }
        length |= (size_t) (bp[pos] & 0x7F) << shift;
        shift += 7;
    } while (bp[pos++] & 0x80);
    if (pos + length > avail) {
        return 0;
    }
    return pos + length;
}

/*
    Write raw bytes to the client. Loops on partial writes: the client drains the socket promptly.
 */
static inline void fbSend(FakeBroker *fb, cvoid *bytes, size_t len)
{
    cuchar *bp;
    ssize  written;

    bp = (cuchar*) bytes;
    while (fb->client && len > 0) {
        written = rWriteSocketSync(fb->client, bp, len);
        if (written < 0) {
            return;
        }
        if (written == 0) {
            rSleep(1);
            continue;
        }
        bp += written;
        len -= (size_t) written;
    }
}

/*
    Send a two byte acknowledgement packet carrying a packet identifier
 */
static inline void fbAck(FakeBroker *fb, int type, int flags, int id)
{
    uchar ack[4];

    ack[0] = (uchar) ((type << 4) | flags);
    ack[1] = 2;
    ack[2] = (uchar) (id >> 8);
    ack[3] = (uchar) id;
    fbSend(fb, ack, sizeof(ack));
}

/*
    Encode a PUBLISH into buf, which must hold fbPublishSize() bytes. Returns the packet length.
    Extra flags are MQTT_RETAIN and MQTT_DUP.
 */
static inline size_t fbBuildPublish(uchar *buf, cchar *topic, cvoid *data, size_t len, int qos, int id, int flags)
{
    size_t topicLen, size, pos;

    topicLen = slen(topic);
    size = 2 + topicLen + (qos > 0 ? 2 : 0) + len;
    buf[0] = (uchar) ((MQTT_PACKET_PUBLISH << 4) | (qos << 1) | flags);
    pos = 1 + fbEncodeLength(&buf[1], size);
    buf[pos++] = (uchar) (topicLen >> 8);
    buf[pos++] = (uchar) topicLen;
    memcpy(&buf[pos], topic, topicLen);
    pos += topicLen;
    if (qos > 0) {
        buf[pos++] = (uchar) (id >> 8);
        buf[pos++] = (uchar) id;
    }
    if (len > 0) {
        memcpy(&buf[pos], data, len);
        pos += len;
    }
    return pos;
}

/*
    Buffer size required by fbBuildPublish for a topic and payload of the given sizes
 */
static inline size_t fbPublishSize(cchar *topic, size_t len)
{
    return slen(topic) + len + 9;
}

/*
    Send a PUBLISH to the client. Extra flags are MQTT_RETAIN and MQTT_DUP.
 */
static inline void fbPublish(FakeBroker *fb, cchar *topic, cvoid *data, size_t len, int qos, int id, int flags)
{
    uchar *buf;

    buf = rAlloc(fbPublishSize(topic, len));
    fbSend(fb, buf, fbBuildPublish(buf, topic, data, len, qos, id, flags));
    rFree(buf);
}

/*
    Record the complete packets in the input buffer and answer them
 */
static inline void fbProcess(FakeBroker *fb)
{
    FakePacket *pkt;
    cuchar     *bp, *body;
    uchar      ack[5];
    size_t     avail, len, hlen, topicLen;
    int        qos;

    while (fb->client) {
        avail = (size_t) rGetBufLength(fb->in);
        bp = (cuchar*) rGetBufStart(fb->in);
        if ((len = fbPacketLength(bp, avail)) == 0) {
            break;
        }
        pkt = rAllocType(FakePacket);
        pkt->data = rMemdup(bp, len);
        pkt->len = len;
        pkt->type = bp[0] >> 4;
        pkt->flags = bp[0] & 0x0F;

        for (hlen = 1; bp[hlen] & 0x80; hlen++) {
        }
        body = &bp[hlen + 1];
        qos = (pkt->flags >> 1) & 0x3;

        switch (pkt->type) {
        case MQTT_PACKET_PUBLISH:
            topicLen = (size_t) ((body[0] << 8) | body[1]);
            if (qos > 0) {
                pkt->id = (body[2 + topicLen] << 8) | body[3 + topicLen];
            }
            break;
        case MQTT_PACKET_PUB_ACK:
        case MQTT_PACKET_PUB_REC:
        case MQTT_PACKET_PUB_REL:
        case MQTT_PACKET_PUB_COMP:
        case MQTT_PACKET_SUB:
        case MQTT_PACKET_UNSUB:
            pkt->id = (body[0] << 8) | body[1];
            break;
        default:
            break;
        }
        rAddItem(fb->packets, pkt);
        rAdjustBufStart(fb->in, (ssize) len);

        switch (pkt->type) {
        case MQTT_PACKET_CONNECT:
            if (fb->ackConnect) {
                ack[0] = MQTT_PACKET_CONN_ACK << 4;
                ack[1] = 2;
                ack[2] = 0;
                ack[3] = (uchar) fb->connackCode;
                fbSend(fb, ack, 4);
            }
            break;
        case MQTT_PACKET_PUBLISH:
            if (fb->ackPublish && qos == 1) {
                fbAck(fb, MQTT_PACKET_PUB_ACK, 0, pkt->id);
            } else if (fb->ackPublish && qos == 2) {
                fbAck(fb, MQTT_PACKET_PUB_REC, 0, pkt->id);
            }
            break;
        case MQTT_PACKET_PUB_REC:
            if (fb->ackPublish) {
                fbAck(fb, MQTT_PACKET_PUB_REL, 2, pkt->id);
            }
            break;
        case MQTT_PACKET_PUB_REL:
            if (fb->ackPublish) {
                fbAck(fb, MQTT_PACKET_PUB_COMP, 0, pkt->id);
            }
            break;
        case MQTT_PACKET_SUB:
            if (fb->ackSubscribe) {
                ack[0] = MQTT_PACKET_SUB_ACK << 4;
                ack[1] = 3;
                ack[2] = (uchar) (pkt->id >> 8);
                ack[3] = (uchar) pkt->id;
                ack[4] = (uchar) fb->subackCode;
                fbSend(fb, ack, 5);
            }
            break;
        case MQTT_PACKET_UNSUB:
            if (fb->ackSubscribe) {
                fbAck(fb, MQTT_PACKET_UNSUB_ACK, 0, pkt->id);
            }
            break;
        case MQTT_PACKET_PING:
            if (fb->ackPing) {
                ack[0] = MQTT_PACKET_PING_ACK << 4;
                ack[1] = 0;
                fbSend(fb, ack, 2);
            }
            break;
        case MQTT_PACKET_DISCONNECT:
            //  A broker closes the connection on DISCONNECT
            fb->closing = 1;
            break;
        default:
            break;
        }
    }
    rResetBufIfEmpty(fb->in);
}

/*
    Connection handler. Runs on its own fiber for the life of the client connection.
 */
static void fbHandler(cvoid *arg, RSocket *sp)
{
    FakeBroker *fb;
    char       buf[2048];
    ssize      nbytes;

    fb = (FakeBroker*) arg;
    fb->client = sp;
    fb->running = 1;
    fb->accepted++;

    while (!fb->closing) {
        nbytes = rReadSocket(sp, buf, sizeof(buf), rGetTicks() + 50);
        if (nbytes == R_ERR_TIMEOUT) {
            continue;
        }
        if (nbytes <= 0) {
            break;
        }
        rPutBlockToBuf(fb->in, buf, (size_t) nbytes);
        fbProcess(fb);
    }
    fb->client = NULL;
    rFreeSocket(sp);
    rFlushBuf(fb->in);
    fb->running = 0;
}

static inline void fbClearPackets(FakeBroker *fb)
{
    FakePacket *pkt;
    int        next;

    for (ITERATE_ITEMS(fb->packets, pkt, next)) {
        rFree(pkt->data);
        rFree(pkt);
    }
    rClearList(fb->packets);
}

/*
    Start a broker on an ephemeral loopback port
 */
static inline FakeBroker *fbOpen(void)
{
    FakeBroker *fb;
    char       ip[64];

    fb = rAllocType(FakeBroker);
    fb->packets = rAllocList(0, 0);
    fb->in = rAllocBuf(4096);
    fb->subackCode = MQTT_SUBACK_SUCCESS_MAX_QOS_1;
    fb->ackConnect = 1;
    fb->ackPublish = 1;
    fb->ackSubscribe = 1;
    fb->ackPing = 1;
    fb->listen = rAllocSocket();
    if (rListenSocket(fb->listen, "127.0.0.1", 0, fbHandler, fb) < 0) {
        tfail("Cannot listen for the fake broker");
        return NULL;
    }
    rGetSocketAddr(fb->listen, ip, sizeof(ip), &fb->port);
    return fb;
}

/*
    Connect a client socket to the broker and wait for the broker to accept it
 */
static inline RSocket *fbConnect(FakeBroker *fb)
{
    RSocket *sock;

    WAIT_UNTIL(!fb->running, 2000);
    fb->closing = 0;
    sock = rAllocSocket();
    if (rConnectSocket(sock, "127.0.0.1", fb->port, 0) < 0) {
        rFreeSocket(sock);
        return NULL;
    }
    WAIT_UNTIL(fb->client != NULL, 2000);
    return sock;
}

/*
    Drop the client connection from the broker side
 */
static inline void fbDrop(FakeBroker *fb)
{
    fb->closing = 1;
    WAIT_UNTIL(!fb->running, 2000);
    fb->closing = 0;
}

static inline void fbClose(FakeBroker *fb)
{
    fb->closing = 1;
    WAIT_UNTIL(!fb->running, 2000);
    rFreeSocket(fb->listen);
    fbClearPackets(fb);
    rFreeList(fb->packets);
    rFreeBuf(fb->in);
    rFree(fb);
}

static inline int fbCount(FakeBroker *fb, int type)
{
    FakePacket *pkt;
    int        count, next;

    count = 0;
    for (ITERATE_ITEMS(fb->packets, pkt, next)) {
        if (pkt->type == type) {
            count++;
        }
    }
    return count;
}

/*
    Return the index-th packet of the given type, or NULL
 */
static inline FakePacket *fbGet(FakeBroker *fb, int type, int index)
{
    FakePacket *pkt;
    int        next;

    for (ITERATE_ITEMS(fb->packets, pkt, next)) {
        if (pkt->type == type && index-- == 0) {
            return pkt;
        }
    }
    return NULL;
}

static inline FakePacket *fbLast(FakeBroker *fb, int type)
{
    int count;

    count = fbCount(fb, type);
    return count > 0 ? fbGet(fb, type, count - 1) : NULL;
}

/*
    Wait until the broker has received at least count packets of the given type
 */
static inline bool fbWait(FakeBroker *fb, int type, int count, Ticks timeout)
{
    WAIT_UNTIL(fbCount(fb, type) >= count, timeout);
    return fbCount(fb, type) >= count;
}

#endif /* _h_FAKE_BROKER */

/*
    Copyright (c) Embedthis Pty Ltd. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
