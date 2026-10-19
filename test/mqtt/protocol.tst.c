/*
    protocol.tst.c - MQTT protocol conformance tests against a scripted broker

    Every test runs the client against the fake broker in fakebroker.h, so the bytes on the wire
    can be inspected and the client's packet parser can be fed malformed input.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "fakebroker.h"

/*********************************** Locals ***********************************/

typedef struct BadPacket {
    cchar *name;
    uchar bytes[8];
    int   len;
    int   error;
} BadPacket;

static FakeBroker *fb;
static int        events[8];
static int        received;
static char       lastTopic[MQTT_MAX_TOPIC_SIZE];
static char       lastData[512];
static size_t     lastDataSize;
static int        lastQos;
static int        lastRetain;
static int        lastDup;

/************************************ Code ************************************/

static void eventProc(Mqtt *mq, int event)
{
    if (event > 0 && event < 8) {
        events[event]++;
    }
}

static void onMessage(const MqttRecv *rp)
{
    received++;
    sncopy(lastTopic, sizeof(lastTopic), rp->topic, rp->topicSize);
    lastDataSize = rp->dataSize;
    if (rp->dataSize > 0 && rp->dataSize < sizeof(lastData)) {
        memcpy(lastData, rp->data, rp->dataSize);
        lastData[rp->dataSize] = '\0';
    } else {
        lastData[0] = '\0';
    }
    lastQos = rp->qos;
    lastRetain = rp->retain;
    lastDup = rp->dup;
}

static void resetState(void)
{
    memset(events, 0, sizeof(events));
    received = 0;
    lastTopic[0] = '\0';
    lastData[0] = '\0';
    lastDataSize = 0;
    lastQos = lastRetain = lastDup = 0;
    fbClearPackets(fb);
}

/*
    Allocate a client and connect it to the fake broker, waiting for the CONNACK
 */
static Mqtt *openClient(cchar *id, RSocket **sockp)
{
    Mqtt    *mq;
    RSocket *sock;

    resetState();
    sock = fbConnect(fb);
    ttrue(sock);
    mq = mqttAlloc(id, eventProc);
    ttrue(mq);
    tcall(mqttConnect(mq, sock, 0, MQTT_WAIT_ACK), 0);
    *sockp = sock;
    return mq;
}

static void closeClient(Mqtt *mq, RSocket *sock)
{
    mqttFree(mq);
    rFreeSocket(sock);
    WAIT_UNTIL(!fb->running, 2000);
}

/*
    Check a length-prefixed string at *bpp and advance past it
 */
static void expectString(cuchar **bpp, cchar *expected)
{
    cuchar *bp;
    size_t len;

    bp = *bpp;
    len = (size_t) ((bp[0] << 8) | bp[1]);
    teq((int) len, (int) slen(expected));
    ttrue(memcmp(&bp[2], expected, len) == 0);
    *bpp = &bp[2 + len];
}

static void testConnectPacket(void)
{
    FakePacket *pkt;
    RSocket    *sock;
    Mqtt       *mq;
    cuchar     *bp;
    int        rc;

    resetState();
    mq = mqttAlloc("proto-connect", eventProc);
    ttrue(mq);
    tcall(mqttSetCredentials(mq, "user", "pass"), 0);
    tcall(mqttSetWill(mq, "dev/status", "gone", 4), 0);
    mqttSetKeepAlive(mq, 60 * TPS);

    sock = fbConnect(fb);
    ttrue(sock);
    rc = mqttConnect(mq, sock, MQTT_CONNECT_CLEAN_SESSION | MQTT_CONNECT_WILL_QOS_1 | MQTT_CONNECT_WILL_RETAIN,
                     MQTT_WAIT_ACK);
    teq(rc, 0);
    ttrue(mqttIsConnected(mq));
    teq(events[MQTT_EVENT_CONNECTED], 1);
    teq(mqttGetQueueCount(mq), 0);

    pkt = fbLast(fb, MQTT_PACKET_CONNECT);
    ttrue(pkt);
    bp = pkt->data;
    teq(bp[0], MQTT_PACKET_CONNECT << 4);
    //  Variable header 10, client id 15, will topic 12, will message 6, username 6, password 6
    teq(bp[1], 55);
    teq((int) pkt->len, 57);
    bp += 2;
    ttrue(memcmp(bp, "\x00\x04MQTT\x04", 7) == 0);
    bp += 7;
    teq(bp[0], MQTT_CONNECT_CLEAN_SESSION | MQTT_CONNECT_WILL_FLAG | MQTT_CONNECT_WILL_QOS_1 |
        MQTT_CONNECT_WILL_RETAIN | MQTT_CONNECT_USER_NAME | MQTT_CONNECT_PASSWORD);
    teq((bp[1] << 8) | bp[2], 60);
    bp += 3;
    expectString(&bp, "proto-connect");
    expectString(&bp, "dev/status");
    expectString(&bp, "gone");
    expectString(&bp, "user");
    expectString(&bp, "pass");
    teq((int) (bp - pkt->data), (int) pkt->len);

    closeClient(mq, sock);
}

/*
    Will flags are dropped when no will is defined
 */
static void testConnectWithoutWill(void)
{
    FakePacket *pkt;
    RSocket    *sock;
    Mqtt       *mq;
    int        rc;

    resetState();
    mq = mqttAlloc("proto-nowill", eventProc);
    sock = fbConnect(fb);
    rc = mqttConnect(mq, sock, MQTT_CONNECT_WILL_FLAG | MQTT_CONNECT_WILL_QOS_2 | MQTT_CONNECT_WILL_RETAIN,
                     MQTT_WAIT_ACK);
    teq(rc, 0);
    pkt = fbLast(fb, MQTT_PACKET_CONNECT);
    ttrue(pkt);
    teq(pkt->data[9], 0);
    //  Default keep alive is 20 minutes
    teq((pkt->data[10] << 8) | pkt->data[11], 1200);
    closeClient(mq, sock);
}

static void testConnectBadWillQos(void)
{
    RSocket *sock;
    Mqtt    *mq;
    int     rc;

    resetState();
    mq = mqttAlloc("proto-badwill", eventProc);
    tcall(mqttSetWill(mq, "dev/status", "gone", 4), 0);
    sock = fbConnect(fb);
    rc = mqttConnect(mq, sock, MQTT_CONNECT_WILL_QOS_1 | MQTT_CONNECT_WILL_QOS_2, MQTT_WAIT_ACK);
    teq(rc, R_ERR_BAD_ARGS);
    tfalse(mqttIsConnected(mq));
    closeClient(mq, sock);
}

static void testConnackRefused(void)
{
    RSocket *sock;
    Mqtt    *mq;
    int     rc;

    resetState();
    fb->connackCode = MQTT_CONNACK_REFUSED_NOT_AUTHORIZED;
    mq = mqttAlloc("proto-refused", eventProc);
    sock = fbConnect(fb);
    rc = mqttConnect(mq, sock, 0, MQTT_WAIT_ACK);
    teq(rc, R_ERR_CANT_CONNECT);
    teq(mq->error, R_ERR_CANT_CONNECT);
    tfalse(mqttIsConnected(mq));
    teq(events[MQTT_EVENT_CONNECTED], 0);
    closeClient(mq, sock);

    resetState();
    fb->connackCode = MQTT_CONNACK_REFUSED_IDENTIFIER_REJECTED;
    mq = mqttAlloc("proto-rejected", eventProc);
    sock = fbConnect(fb);
    rc = mqttConnect(mq, sock, 0, MQTT_WAIT_ACK);
    teq(rc, R_ERR_CANT_CONNECT);
    teq(mq->error, R_ERR_CANT_COMPLETE);
    closeClient(mq, sock);

    fb->connackCode = MQTT_CONNACK_ACCEPTED;
}

/*
    Malformed CONNACK packets are fatal to the connection
 */
static void testConnackMalformed(void)
{
    static BadPacket bad[] = {
        { "connack length", { 0x20, 0x03, 0x00, 0x00, 0x00 }, 5, R_ERR_BAD_VALUE },
        { "connack flags", { 0x20, 0x02, 0x02, 0x00 }, 4, R_ERR_BAD_VALUE },
        { "connack code", { 0x20, 0x02, 0x00, 0x06 }, 4, R_ERR_BAD_VALUE },
        { "connack header flags", { 0x21, 0x02, 0x00, 0x00 }, 4, R_ERR_BAD_STATE },
    };
    RSocket *sock;
    Mqtt    *mq;
    int     i;

    fb->ackConnect = 0;
    for (i = 0; i < (int) (sizeof(bad) / sizeof(bad[0])); i++) {
        tinfo("%s", bad[i].name);
        resetState();
        mq = mqttAlloc("proto-connack", eventProc);
        sock = fbConnect(fb);
        mqttConnect(mq, sock, 0, MQTT_WAIT_NONE);
        ttrue(fbWait(fb, MQTT_PACKET_CONNECT, 1, 2000));
        fbSend(fb, bad[i].bytes, (size_t) bad[i].len);
        WAIT_UNTIL(mq->error, 2000);
        teq(mq->error, bad[i].error);
        tfalse(mqttIsConnected(mq));
        closeClient(mq, sock);
    }
    fb->ackConnect = 1;
}

/*
    Malformed packets on an established connection are fatal and raise a disconnect event
 */
static void testMalformedPackets(void)
{
    static BadPacket bad[] = {
        { "type 0", { 0x00, 0x00 }, 2, R_ERR_BAD_DATA },
        { "type 15", { 0xF0, 0x00 }, 2, R_ERR_BAD_DATA },
        { "puback flags", { 0x41, 0x02, 0x00, 0x01 }, 4, R_ERR_BAD_STATE },
        { "pubrel flags", { 0x60, 0x02, 0x00, 0x01 }, 4, R_ERR_BAD_STATE },
        { "length overflow", { 0x30, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, 6, R_ERR_BAD_RESPONSE },
        { "length too big", { 0x30, 0xD0, 0x0F }, 3, R_ERR_BAD_RESPONSE },
        { "puback length", { 0x40, 0x03, 0x00, 0x01, 0x00 }, 5, R_ERR_BAD_RESPONSE },
        { "suback length", { 0x90, 0x02, 0x00, 0x01 }, 4, R_ERR_BAD_RESPONSE },
        { "unsuback length", { 0xB0, 0x03, 0x00, 0x01, 0x00 }, 5, R_ERR_BAD_RESPONSE },
        { "publish short", { 0x30, 0x02, 0x00, 0x00 }, 4, R_ERR_BAD_RESPONSE },
        { "publish topic overflow", { 0x30, 0x04, 0x00, 0x09, 'a', 'b' }, 6, R_ERR_BAD_RESPONSE },
        { "publish qos1 no id", { 0x32, 0x04, 0x00, 0x02, 'a', 'b' }, 6, R_ERR_BAD_RESPONSE },
        { "publish qos3", { 0x36, 0x06, 0x00, 0x01, 'a', 0x00, 0x01, 'x' }, 8, R_ERR_BAD_RESPONSE },
        { "publish empty topic", { 0x30, 0x04, 0x00, 0x00, 'x', 'y' }, 6, R_ERR_BAD_RESPONSE },
        { "publish nul in topic", { 0x30, 0x06, 0x00, 0x03, 'a', 0x00, 'b', 'x' }, 8, R_ERR_BAD_RESPONSE },
        { "publish + in topic", { 0x30, 0x05, 0x00, 0x02, 'a', '+', 'x' }, 7, R_ERR_BAD_RESPONSE },
        { "publish # in topic", { 0x30, 0x05, 0x00, 0x02, 'a', '#', 'x' }, 7, R_ERR_BAD_RESPONSE },
        { "connack repeated", { 0x20, 0x02, 0x00, 0x00 }, 4, R_ERR_BAD_ACK },
        { "puback unknown", { 0x40, 0x02, 0x00, 0x63 }, 4, R_ERR_BAD_ACK },
        { "pubrec unknown", { 0x50, 0x02, 0x00, 0x63 }, 4, R_ERR_BAD_ACK },
        { "pubrel unknown", { 0x62, 0x02, 0x00, 0x63 }, 4, R_ERR_BAD_ACK },
        { "pubcomp unknown", { 0x70, 0x02, 0x00, 0x63 }, 4, R_ERR_BAD_ACK },
        { "suback unknown", { 0x90, 0x03, 0x00, 0x63, 0x01 }, 5, R_ERR_BAD_ACK },
        { "unsuback unknown", { 0xB0, 0x02, 0x00, 0x63 }, 4, R_ERR_BAD_ACK },
        { "pingresp unknown", { 0xD0, 0x00 }, 2, R_ERR_BAD_ACK },
    };
    RSocket *sock;
    Mqtt    *mq;
    int     i;

    for (i = 0; i < (int) (sizeof(bad) / sizeof(bad[0])); i++) {
        tinfo("%s", bad[i].name);
        mq = openClient("proto-bad", &sock);
        mqttSetMessageSize(mq, 1000);
        tcall(mqttSubscribe(mq, onMessage, 1, MQTT_WAIT_ACK, "a"), 0);
        fbSend(fb, bad[i].bytes, (size_t) bad[i].len);
        WAIT_UNTIL(mq->error, 2000);
        teq(mq->error, bad[i].error);
        WAIT_UNTIL(events[MQTT_EVENT_DISCONNECT], 2000);
        teq(events[MQTT_EVENT_DISCONNECT], 1);
        tfalse(mqttIsConnected(mq));
        teq(received, 0);
        closeClient(mq, sock);
    }
}

static void testIncomingPublish(void)
{
    static uchar rel[] = { 0x62, 0x02, 0x00, 0x09 };
    RSocket      *sock;
    Mqtt         *mq;
    char         *big;

    mq = openClient("proto-incoming", &sock);
    tcall(mqttSubscribe(mq, onMessage, 2, MQTT_WAIT_ACK, "in/#"), 0);
    teq(fbCount(fb, MQTT_PACKET_SUB), 1);

    //  QoS 0 is delivered without an acknowledgement
    fbPublish(fb, "in/q0", "zero", 4, 0, 0, 0);
    WAIT_UNTIL(received == 1, 2000);
    teq(received, 1);
    tmatch(lastTopic, "in/q0");
    tmatch(lastData, "zero");
    teq(lastQos, 0);
    teq(mqttGetQueueCount(mq), 0);
    teq(fbCount(fb, MQTT_PACKET_PUB_ACK), 0);

    //  QoS 1 is acknowledged with PUBACK carrying the same id
    fbPublish(fb, "in/q1", "one", 3, 1, 7, MQTT_RETAIN);
    WAIT_UNTIL(received == 2, 2000);
    teq(received, 2);
    tmatch(lastData, "one");
    teq(lastQos, 1);
    teq(lastRetain, 1);
    ttrue(fbWait(fb, MQTT_PACKET_PUB_ACK, 1, 2000));
    teq(fbLast(fb, MQTT_PACKET_PUB_ACK)->id, 7);

    /*
        QoS 2 is acknowledged with PUBREC, then PUBCOMP after the PUBREL. The broker must not
        answer the PUBREC automatically here: the duplicate below has to arrive before the PUBREL.
     */
    fb->ackPublish = 0;
    fbPublish(fb, "in/q2", "two", 3, 2, 9, MQTT_DUP);
    WAIT_UNTIL(received == 3, 2000);
    teq(received, 3);
    teq(lastQos, 2);
    teq(lastDup, 1);
    ttrue(fbWait(fb, MQTT_PACKET_PUB_REC, 1, 2000));
    teq(fbLast(fb, MQTT_PACKET_PUB_REC)->id, 9);

    //  A repeated QoS 2 PUBLISH before the PUBREL is not delivered again
    fbPublish(fb, "in/q2", "two", 3, 2, 9, MQTT_DUP);
    rSleep(100);
    teq(received, 3);

    fbSend(fb, rel, sizeof(rel));
    fb->ackPublish = 1;
    ttrue(fbWait(fb, MQTT_PACKET_PUB_COMP, 1, 2000));
    teq(fbLast(fb, MQTT_PACKET_PUB_COMP)->id, 9);
    WAIT_UNTIL(mqttGetQueueCount(mq) == 0, 2000);
    teq(mqttGetQueueCount(mq), 0);

    //  Empty payload
    fbPublish(fb, "in/empty", NULL, 0, 0, 0, 0);
    WAIT_UNTIL(received == 4, 2000);
    teq(received, 4);
    teq((int) lastDataSize, 0);

    //  A topic with no subscription is acknowledged but not delivered
    fbPublish(fb, "out/x", "no", 2, 1, 11, 0);
    ttrue(fbWait(fb, MQTT_PACKET_PUB_ACK, 2, 2000));
    teq(fbLast(fb, MQTT_PACKET_PUB_ACK)->id, 11);
    teq(received, 4);

    //  A payload needing a two byte remaining length
    big = rAlloc(3000);
    memset(big, 'x', 3000);
    fbPublish(fb, "in/big", big, 3000, 0, 0, 0);
    WAIT_UNTIL(received == 5, 2000);
    teq(received, 5);
    teq((int) lastDataSize, 3000);
    rFree(big);

    closeClient(mq, sock);
}


static void testTopicLevels(void)
{
    RSocket *sock;
    Mqtt    *mq;

    mq = openClient("proto-levels", &sock);
    tcall(mqttSubscribe(mq, onMessage, 0, MQTT_WAIT_ACK, "lv/a/b"), 0);
    tcall(mqttSubscribe(mq, onMessage, 0, MQTT_WAIT_ACK, "pl/+/x"), 0);
    tcall(mqttSubscribe(mq, onMessage, 0, MQTT_WAIT_ACK, "+/first"), 0);
    tcall(mqttSubscribe(mq, onMessage, 0, MQTT_WAIT_ACK, "last/+"), 0);

    fbPublish(fb, "lv//a/b", "1", 1, 0, 0, 0);
    fbPublish(fb, "lv/a//b", "2", 1, 0, 0, 0);
    fbPublish(fb, "lv/a/b/", "3", 1, 0, 0, 0);
    fbPublish(fb, "/lv/a/b", "4", 1, 0, 0, 0);
    fbPublish(fb, "lv/a/b", "5", 1, 0, 0, 0);
    WAIT_UNTIL(received == 1, 2000);
    rSleep(50);
    teq(received, 1);
    tmatch(lastTopic, "lv/a/b");

    fbPublish(fb, "pl//x", "6", 1, 0, 0, 0);
    WAIT_UNTIL(received == 2, 2000);
    teq(received, 2);
    tmatch(lastTopic, "pl//x");

    fbPublish(fb, "/first", "7", 1, 0, 0, 0);
    WAIT_UNTIL(received == 3, 2000);
    teq(received, 3);
    tmatch(lastTopic, "/first");

    fbPublish(fb, "last/", "8", 1, 0, 0, 0);
    WAIT_UNTIL(received == 4, 2000);
    teq(received, 4);
    tmatch(lastTopic, "last/");
    tfalse(mq->error);

    closeClient(mq, sock);
}

static char ignoredLog[256];

static void captureLog(cchar *type, cchar *source, cchar *msg)
{
    if (scontains(msg, "not subscribed")) {
        scopy(ignoredLog, sizeof(ignoredLog), msg);
    }
}


static void testUnsubscribedLog(void)
{
    RLogHandler prior;
    RSocket     *sock;
    Mqtt        *mq;

    mq = openClient("proto-unsub-log", &sock);
    tcall(mqttSubscribe(mq, onMessage, 0, MQTT_WAIT_ACK, "in/#"), 0);

    ignoredLog[0] = '\0';
    rSetLogFilter("all", "mqtt", 1);
    prior = rSetLogHandler(captureLog);
    fbPublish(fb, "out/x", "SECRET", 6, 1, 12, 0);
    ttrue(fbWait(fb, MQTT_PACKET_PUB_ACK, 1, 2000));
    rSleep(20);
    rSetLogHandler(prior);
    rSetLogFilter("error,info", "all", 1);

    teq(received, 0);
    ttrue(scontains(ignoredLog, "not subscribed to out/x"));
    tfalse(scontains(ignoredLog, "SECRET"));
    ttrue(scontains(ignoredLog, "6 byte"));

    closeClient(mq, sock);
}

/*
    A read that fills the receive buffer exactly leaves no room for the trailing null. Growing the
    buffer to make room compacts the pending bytes over, and then frees, the block the unpacked
    MqttRecv topic and data point into. The subscriber must still be given what arrived.
 */
static void testReceiveBufferBoundary(void)
{
    RSocket *sock;
    Mqtt    *mq;
    uchar   *wire, *tail;
    char    *payload;
    size_t  space, total, first, fill, tailSize;
    int     delta;

    for (delta = -1; delta <= 1; delta++) {
        mq = openClient("proto-boundary", &sock);
        tcall(mqttSubscribe(mq, onMessage, 0, MQTT_WAIT_ACK, "in/#"), 0);

        //  The next read is issued after the empty buffer has been reset, so all of it is free
        teq((int) rGetBufLength(mq->buf), 0);
        space = rGetBufSize(mq->buf);
        total = space + (size_t) delta;

        /*
            One complete PUBLISH followed by the start of a second, sized to fill the read exactly.
            The trailing bytes are what a compaction moves over the first packet's topic and data.
         */
        wire = rAlloc(total);
        first = fbBuildPublish(wire, "in/edge", "edge", 4, 0, 0, 0);
        fill = total - first;

        payload = rAlloc(fill);
        memset(payload, 'x', fill);
        tail = rAlloc(fbPublishSize("in/tail", fill));
        tailSize = fbBuildPublish(tail, "in/tail", payload, fill, 0, 0, 0);
        memcpy(&wire[first], tail, fill);

        fbSend(fb, wire, total);
        WAIT_UNTIL(received == 1, 2000);
        teq(received, 1);
        tmatch(lastTopic, "in/edge");
        tmatch(lastData, "edge");

        //  The withheld remainder of the second packet completes it
        fbSend(fb, &tail[fill], tailSize - fill);
        WAIT_UNTIL(received == 2, 2000);
        teq(received, 2);
        tmatch(lastTopic, "in/tail");
        teq((int) lastDataSize, (int) fill);

        rFree(payload);
        rFree(tail);
        rFree(wire);
        closeClient(mq, sock);
    }
}

/*
    Packets split across reads and packets coalesced into one read are both reassembled
 */
static void testPacketFraming(void)
{
    static uchar pub[] = { 0x30, 0x0A, 0x00, 0x04, 'i', 'n', '/', 'p', 'd', 'a', 't', 'a' };
    uchar        two[sizeof(pub) * 2];
    RSocket      *sock;
    Mqtt         *mq;

    mq = openClient("proto-framing", &sock);
    tcall(mqttSubscribe(mq, onMessage, 1, MQTT_WAIT_ACK, "in/#"), 0);

    //  Header byte alone, then the length, then the body in two pieces
    fbSend(fb, pub, 1);
    rSleep(50);
    fbSend(fb, &pub[1], 1);
    rSleep(50);
    fbSend(fb, &pub[2], 5);
    rSleep(50);
    teq(received, 0);
    fbSend(fb, &pub[7], sizeof(pub) - 7);
    WAIT_UNTIL(received == 1, 2000);
    teq(received, 1);
    tmatch(lastTopic, "in/p");
    tmatch(lastData, "data");

    memcpy(two, pub, sizeof(pub));
    memcpy(&two[sizeof(pub)], pub, sizeof(pub));
    fbSend(fb, two, sizeof(two));
    WAIT_UNTIL(received == 3, 2000);
    teq(received, 3);

    closeClient(mq, sock);
}

static void testPing(void)
{
    RSocket *sock;
    Mqtt    *mq;

    mq = openClient("proto-ping", &sock);
    tcall(mqttPing(mq), 0);
    ttrue(fbWait(fb, MQTT_PACKET_PING, 1, 2000));
    teq((int) fbLast(fb, MQTT_PACKET_PING)->len, 2);
    WAIT_UNTIL(mqttGetQueueCount(mq) == 0, 2000);
    teq(mqttGetQueueCount(mq), 0);

    //  Without a PINGRESP the request stays queued awaiting its ack
    fb->ackPing = 0;
    tcall(mqttPing(mq), 0);
    ttrue(fbWait(fb, MQTT_PACKET_PING, 2, 2000));
    rSleep(50);
    teq(mqttGetQueueCount(mq), 1);
    fb->ackPing = 1;
    closeClient(mq, sock);
}

/*
    DISCONNECT has no payload and the broker closes the connection on receipt
 */
static void testDisconnect(void)
{
    FakePacket *pkt;
    RSocket    *sock;
    Mqtt       *mq;

    mq = openClient("proto-disconnect", &sock);
    tcall(mqttDisconnect(mq), 0);
    ttrue(fbWait(fb, MQTT_PACKET_DISCONNECT, 1, 2000));
    pkt = fbLast(fb, MQTT_PACKET_DISCONNECT);
    teq((int) pkt->len, 2);
    teq(pkt->data[0], MQTT_PACKET_DISCONNECT << 4);
    teq(pkt->data[1], 0);
    WAIT_UNTIL(events[MQTT_EVENT_DISCONNECT], 2000);
    teq(events[MQTT_EVENT_DISCONNECT], 1);
    tfalse(mqttIsConnected(mq));
    closeClient(mq, sock);
}

/*
    A refused subscription is reported to the caller and does not affect the connection
 */
static void testSubackFailure(void)
{
    RSocket *sock;
    Mqtt    *mq;
    int     rc;

    mq = openClient("proto-suback", &sock);
    fb->subackCode = MQTT_SUBACK_FAILURE;
    rc = mqttSubscribe(mq, onMessage, 1, MQTT_WAIT_ACK, "deny/x");
    ttrue(rc < 0);
    fb->subackCode = MQTT_SUBACK_SUCCESS_MAX_QOS_1;
    teq(mq->error, 0);
    ttrue(mqttIsConnected(mq));
    tcall(mqttPublish(mq, "x", 1, 1, MQTT_WAIT_ACK, "still/up"), 0);
    teq(fbCount(fb, MQTT_PACKET_PUBLISH), 1);
    closeClient(mq, sock);
}

/*
    Bad arguments are rejected without harming the connection
 */
static void testBadArgsKeepConnection(void)
{
    RSocket *sock;
    Mqtt    *mq;
    char    *big;

    mq = openClient("proto-badargs", &sock);
    tcall(mqttPublish(mq, "x", 1, 0, MQTT_WAIT_NONE, "bad/+"), R_ERR_BAD_ARGS);
    tcall(mqttPublish(mq, "x", 1, 0, MQTT_WAIT_NONE, "bad/#"), R_ERR_BAD_ARGS);
    tcall(mqttSubscribe(mq, onMessage, 1, MQTT_WAIT_NONE, "bad#"), R_ERR_BAD_ARGS);
    tcall(mqttSubscribeMaster(mq, 1, MQTT_WAIT_NONE, "bad/+x"), R_ERR_BAD_ARGS);
    big = rAlloc(70001);
    memset(big, 'a', 70000);
    big[70000] = '\0';
    tcall(mqttUnsubscribeMaster(mq, big, MQTT_WAIT_NONE), R_ERR_BAD_ARGS);
    rFree(big);
    teq(mq->error, 0);
    ttrue(mqttIsConnected(mq));
    tcall(mqttPublish(mq, "x", 1, 1, MQTT_WAIT_ACK, "still/up"), 0);
    teq(fbCount(fb, MQTT_PACKET_PUBLISH), 1);
    closeClient(mq, sock);
}

static void testServerClose(void)
{
    RSocket *sock;
    Mqtt    *mq;

    mq = openClient("proto-close", &sock);
    fbDrop(fb);
    WAIT_UNTIL(events[MQTT_EVENT_DISCONNECT], 2000);
    teq(events[MQTT_EVENT_DISCONNECT], 1);
    tfalse(mqttIsConnected(mq));
    teq(mq->error, R_ERR_NETWORK);
    tcall(mqttPublish(mq, "x", 1, 0, MQTT_WAIT_NONE, "gone"), R_ERR_NETWORK);
    tcall(mqttPing(mq), R_ERR_NETWORK);
    closeClient(mq, sock);
}

static void testUnsubscribe(void)
{
    RSocket *sock;
    Mqtt    *mq;

    mq = openClient("proto-unsub", &sock);
    tcall(mqttSubscribe(mq, onMessage, 1, MQTT_WAIT_ACK, "u/a"), 0);
    teq(rGetListLength(mq->topics), 1);
    tcall(mqttUnsubscribe(mq, "u/a", MQTT_WAIT_ACK), 0);
    teq(fbCount(fb, MQTT_PACKET_UNSUB), 1);
    teq(rGetListLength(mq->topics), 0);
    teq(mqttGetQueueCount(mq), 0);
    closeClient(mq, sock);
}

/*
    Master subscriptions cover only topics under the master prefix at a level boundary, and
    unsubscribing the master removes it and the local subscriptions under it
 */
static void testMasterSubscription(void)
{
    RSocket *sock;
    Mqtt    *mq;

    mq = openClient("proto-master", &sock);
    tcall(mqttSubscribeMaster(mq, 1, MQTT_WAIT_ACK, "device/#"), 0);
    teq(fbCount(fb, MQTT_PACKET_SUB), 1);
    teq(rGetListLength(mq->masterTopics), 1);
    tmatch(rGetItem(mq->masterTopics, 0), "device");

    //  Under the master: no protocol subscription
    tcall(mqttSubscribe(mq, onMessage, 1, MQTT_WAIT_ACK, "device/1/status"), 0);
    teq(fbCount(fb, MQTT_PACKET_SUB), 1);

    //  Shares the prefix but not the level: must go to the broker
    tcall(mqttSubscribe(mq, onMessage, 1, MQTT_WAIT_ACK, "devices/1/status"), 0);
    teq(fbCount(fb, MQTT_PACKET_SUB), 2);
    teq(rGetListLength(mq->topics), 2);

    tcall(mqttUnsubscribeMaster(mq, "device/#", MQTT_WAIT_ACK), 0);
    teq(fbCount(fb, MQTT_PACKET_UNSUB), 1);
    teq(rGetListLength(mq->masterTopics), 0);
    teq(rGetListLength(mq->topics), 1);
    tmatch(((MqttTopic*) rGetItem(mq->topics, 0))->topic, "devices/1/status");

    //  With the master gone, subscriptions under it go to the broker again
    tcall(mqttSubscribe(mq, onMessage, 1, MQTT_WAIT_ACK, "device/2/status"), 0);
    teq(fbCount(fb, MQTT_PACKET_SUB), 3);
    closeClient(mq, sock);
}

static void fiberMain(void *data)
{
    fb = fbOpen();
    ttrue(fb);

    testConnectPacket();
    testConnectWithoutWill();
    testConnectBadWillQos();
    testConnackRefused();
    testConnackMalformed();
    testMalformedPackets();
    testIncomingPublish();
    testTopicLevels();
    testUnsubscribedLog();
    testReceiveBufferBoundary();
    testPacketFraming();
    testPing();
    testDisconnect();
    testSubackFailure();
    testBadArgsKeepConnection();
    testServerClose();
    testUnsubscribe();
    testMasterSubscription();

    fbClose(fb);
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
    This is proprietary software and requires a commercial license from the author.
 */
