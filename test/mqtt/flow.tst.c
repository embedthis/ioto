/*
    flow.tst.c - MQTT message flow tests: QoS state machines, retransmission, keep-alive,
    throttling, on-demand connection and reconnection, against the scripted broker.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "fakebroker.h"

/*********************************** Locals ***********************************/

typedef struct Publisher {
    Mqtt *mq;
    int  qos;
    int  wait;
    int  done;
    int  rc;
} Publisher;

static FakeBroker *fb;
static int        events[8];
static RSocket    *attachSock;
static int        freed;

/************************************ Code ************************************/

static void eventProc(Mqtt *mq, int event)
{
    if (event > 0 && event < 8) {
        events[event]++;
    }
    if (event == MQTT_EVENT_ATTACH) {
        //  On-demand connection: supply a socket and connect
        attachSock = fbConnect(fb);
        mqttConnect(mq, attachSock, 0, MQTT_WAIT_ACK);
    }
}

static void onMessage(const MqttRecv *rp)
{
}

static Mqtt *openClient(cchar *id, RSocket **sockp)
{
    Mqtt    *mq;
    RSocket *sock;

    memset(events, 0, sizeof(events));
    fbClearPackets(fb);
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
    Publish from a separate fiber so a publish that never completes cannot hang the test
 */
static void publisherProc(Publisher *pub)
{
    pub->rc = mqttPublish(pub->mq, "payload", 7, pub->qos, pub->wait, "flow/%d", pub->qos);
    pub->done = 1;
}

/*
    An unacknowledged QoS 1 publish is resent with the DUP flag after the message timeout
 */
static void testQos1Retransmit(void)
{
    FakePacket *first, *second;
    RSocket    *sock;
    Mqtt       *mq;

    mq = openClient("flow-retransmit", &sock);
    fb->ackPublish = 0;
    mq->msgTimeout = 200;
    tcall(mqttPublish(mq, "payload", 7, 1, MQTT_WAIT_SENT, "flow/retry"), 0);
    ttrue(fbWait(fb, MQTT_PACKET_PUBLISH, 1, 2000));
    teq(mqttGetQueueCount(mq), 1);

    //  Retransmission is driven by the socket wait handler, so give it an event after the timeout
    rSleep(300);
    fbPublish(fb, "unrelated", "x", 1, 0, 0, 0);
    ttrue(fbWait(fb, MQTT_PACKET_PUBLISH, 2, 2000));
    first = fbGet(fb, MQTT_PACKET_PUBLISH, 0);
    second = fbGet(fb, MQTT_PACKET_PUBLISH, 1);
    teq(first->flags & MQTT_DUP, 0);
    teq(second->flags & MQTT_DUP, MQTT_DUP);
    teq(second->id, first->id);
    teq((int) second->len, (int) first->len);

    fbAck(fb, MQTT_PACKET_PUB_ACK, 0, first->id);
    WAIT_UNTIL(mqttGetQueueCount(mq) == 0, 2000);
    teq(mqttGetQueueCount(mq), 0);
    fb->ackPublish = 1;
    closeClient(mq, sock);
}

/*
    A QoS 2 publish waiting for its acknowledgement completes after PUBREC, PUBREL and PUBCOMP
 */
static void testQos2PublishWaitAck(void)
{
    Publisher pub;
    RSocket   *sock;
    Mqtt      *mq;

    mq = openClient("flow-qos2", &sock);
    memset(&pub, 0, sizeof(pub));
    pub.mq = mq;
    pub.qos = 2;
    pub.wait = MQTT_WAIT_ACK;
    rStartEvent((REventProc) publisherProc, &pub, 0);
    WAIT_UNTIL(pub.done, 3000);
    ttrue(pub.done);
    teq(pub.rc, 0);
    teq(fbCount(fb, MQTT_PACKET_PUBLISH), 1);
    teq(fbCount(fb, MQTT_PACKET_PUB_REL), 1);
    teq(fbLast(fb, MQTT_PACKET_PUB_REL)->id, fbLast(fb, MQTT_PACKET_PUBLISH)->id);
    teq(fbLast(fb, MQTT_PACKET_PUB_REL)->flags, 2);
    WAIT_UNTIL(mqttGetQueueCount(mq) == 0, 2000);
    teq(mqttGetQueueCount(mq), 0);
    closeClient(mq, sock);
}

/*
    Only one QoS 2 publish is in flight at a time
 */
static void testQos2Inflight(void)
{
    RSocket *sock;
    Mqtt    *mq;
    int     id;

    mq = openClient("flow-inflight", &sock);
    fb->ackPublish = 0;
    tcall(mqttPublish(mq, "one", 3, 2, MQTT_WAIT_NONE, "flow/a"), 0);
    tcall(mqttPublish(mq, "two", 3, 2, MQTT_WAIT_NONE, "flow/b"), 0);
    ttrue(fbWait(fb, MQTT_PACKET_PUBLISH, 1, 2000));
    rSleep(200);
    teq(fbCount(fb, MQTT_PACKET_PUBLISH), 1);
    teq(mqttGetQueueCount(mq), 2);

    //  Complete the first exchange and the second publish is released
    fb->ackPublish = 1;
    id = fbLast(fb, MQTT_PACKET_PUBLISH)->id;
    fbAck(fb, MQTT_PACKET_PUB_REC, 0, id);
    rSleep(300);
    ttrue(fbWait(fb, MQTT_PACKET_PUB_REL, 1, 2000));
    ttrue(fbWait(fb, MQTT_PACKET_PUBLISH, 2, 2000));
    tneq(fbLast(fb, MQTT_PACKET_PUBLISH)->id, id);
    ttrue(fbWait(fb, MQTT_PACKET_PUB_REL, 2, 2000));
    WAIT_UNTIL(mqttGetQueueCount(mq) == 0, 2000);
    teq(mqttGetQueueCount(mq), 0);
    closeClient(mq, sock);
}

/*
    Messages published with MQTT_WAIT_SENT and MQTT_WAIT_ACK | MQTT_WAIT_FAST leave the queue
    once acknowledged
 */
static void testWaitSent(void)
{
    RSocket *sock;
    Mqtt    *mq;

    mq = openClient("flow-sent", &sock);
    tcall(mqttPublish(mq, "zero", 4, 0, MQTT_WAIT_SENT, "flow/s0"), 0);
    ttrue(fbWait(fb, MQTT_PACKET_PUBLISH, 1, 2000));
    teq(mqttGetQueueCount(mq), 0);

    tcall(mqttPublish(mq, "one", 3, 1, MQTT_WAIT_SENT, "flow/s1"), 0);
    ttrue(fbWait(fb, MQTT_PACKET_PUBLISH, 2, 2000));
    WAIT_UNTIL(mqttGetQueueCount(mq) == 0, 2000);
    teq(mqttGetQueueCount(mq), 0);

    tcall(mqttPublish(mq, "one", 3, 1, MQTT_WAIT_ACK | MQTT_WAIT_FAST, "flow/s2"), 0);
    ttrue(fbWait(fb, MQTT_PACKET_PUBLISH, 3, 2000));
    teq(mqttGetQueueCount(mq), 0);

    tcall(mqttSubscribe(mq, onMessage, 1, MQTT_WAIT_SENT, "flow/sub"), 0);
    ttrue(fbWait(fb, MQTT_PACKET_SUB, 1, 2000));
    WAIT_UNTIL(mqttGetQueueCount(mq) == 0, 2000);
    teq(mqttGetQueueCount(mq), 0);
    closeClient(mq, sock);
}

static void testThrottle(void)
{
    RSocket *sock;
    Mqtt    *mq;
    int     i;

    mq = openClient("flow-throttle", &sock);
    teq((int) mq->throttle, 0);
    mqttThrottle(mq);
    teq((int) mq->throttle, 2 * TPS);
    mqttThrottle(mq);
    teq((int) mq->throttle, 4 * TPS);
    for (i = 0; i < 10; i++) {
        mqttThrottle(mq);
    }
    //  Capped at thirty seconds
    teq((int) mq->throttle, 30 * TPS);

    //  A throttled publish is delayed and the throttle decays with the time since the last publish
    mq->throttle = 20;
    mq->throttleLastPub = rGetTicks() - 10 * TPS;
    tcall(mqttPublish(mq, "x", 1, 0, MQTT_WAIT_SENT, "flow/throttled"), 0);
    teq((int) mq->throttle, 0);
    ttrue(fbWait(fb, MQTT_PACKET_PUBLISH, 1, 2000));
    closeClient(mq, sock);
}

/*
    An idle connection is kept alive with a ping once the keep-alive interval passes
 */
static void testKeepAlivePing(void)
{
    RSocket *sock;
    Mqtt    *mq;

    memset(events, 0, sizeof(events));
    fbClearPackets(fb);
    sock = fbConnect(fb);
    mq = mqttAlloc("flow-keepalive", eventProc);
    mqttSetKeepAlive(mq, 200);
    tcall(mqttConnect(mq, sock, 0, MQTT_WAIT_ACK), 0);
    ttrue(fbWait(fb, MQTT_PACKET_PING, 1, 3000));
    WAIT_UNTIL(mqttGetQueueCount(mq) == 0, 2000);
    teq(mqttGetQueueCount(mq), 0);
    teq(events[MQTT_EVENT_TIMEOUT], 0);
    closeClient(mq, sock);
}

/*
    An idle connection past its timeout raises the timeout event
 */
static void testIdleTimeout(void)
{
    RSocket *sock;
    Mqtt    *mq;

    memset(events, 0, sizeof(events));
    fbClearPackets(fb);
    sock = fbConnect(fb);
    mq = mqttAlloc("flow-idle", eventProc);
    mqttSetTimeout(mq, 300);
    tcall(mqttConnect(mq, sock, 0, MQTT_WAIT_ACK), 0);
    WAIT_UNTIL(events[MQTT_EVENT_TIMEOUT], 3000);
    teq(events[MQTT_EVENT_TIMEOUT], 1);
    teq(fbCount(fb, MQTT_PACKET_PING), 0);
    closeClient(mq, sock);
}

/*
    Publishing without a socket asks the application to attach one
 */
static void testOnDemandAttach(void)
{
    Mqtt *mq;

    memset(events, 0, sizeof(events));
    fbClearPackets(fb);
    attachSock = NULL;
    mq = mqttAlloc("flow-attach", eventProc);
    tcall(mqttPublish(mq, "x", 1, 1, MQTT_WAIT_ACK, "flow/attach"), 0);
    teq(events[MQTT_EVENT_ATTACH], 1);
    teq(events[MQTT_EVENT_CONNECTED], 1);
    ttrue(attachSock != NULL);
    teq(fbCount(fb, MQTT_PACKET_CONNECT), 1);
    teq(fbCount(fb, MQTT_PACKET_PUBLISH), 1);
    ttrue(mqttIsConnected(mq));
    closeClient(mq, attachSock);
}

/*
    Without an attach handler there is nothing to connect
 */
static void testNoAttachHandler(void)
{
    Mqtt *mq;

    mq = mqttAlloc("flow-noattach", NULL);
    tcall(mqttPublish(mq, "x", 1, 1, MQTT_WAIT_NONE, "flow/none"), R_ERR_CANT_WRITE);
    tcall(mqttSubscribe(mq, onMessage, 1, MQTT_WAIT_NONE, "flow/none"), R_ERR_CANT_WRITE);
    tcall(mqttUnsubscribeMaster(mq, "flow/none", MQTT_WAIT_NONE), R_ERR_CANT_WRITE);
    mqttFree(mq);
}

/*
    Freeing the client resumes a fiber waiting for an acknowledgement with an error
 */
static void testFreeWithWaiter(void)
{
    Publisher pub;
    RSocket   *sock;
    Mqtt      *mq;

    mq = openClient("flow-waiter", &sock);
    fb->ackPublish = 0;
    memset(&pub, 0, sizeof(pub));
    pub.mq = mq;
    pub.qos = 1;
    pub.wait = MQTT_WAIT_ACK;
    rStartEvent((REventProc) publisherProc, &pub, 0);
    ttrue(fbWait(fb, MQTT_PACKET_PUBLISH, 1, 2000));
    teq(pub.done, 0);
    mqttFree(mq);
    WAIT_UNTIL(pub.done, 2000);
    teq(pub.done, 1);
    teq(pub.rc, R_ERR_NOT_CONNECTED);
    fb->ackPublish = 1;
    rFreeSocket(sock);
    WAIT_UNTIL(!fb->running, 2000);
}

/*
    After the broker drops the connection, the client can connect again on a new socket
 */
static void testReconnect(void)
{
    RSocket *sock, *sock2;
    Mqtt    *mq;

    mq = openClient("flow-reconnect", &sock);
    fbDrop(fb);
    WAIT_UNTIL(events[MQTT_EVENT_DISCONNECT], 2000);
    teq(events[MQTT_EVENT_DISCONNECT], 1);
    tfalse(mqttIsConnected(mq));
    rFreeSocket(sock);

    sock2 = fbConnect(fb);
    ttrue(sock2);
    tcall(mqttConnect(mq, sock2, 0, MQTT_WAIT_ACK), 0);
    teq(mq->error, 0);
    ttrue(mqttIsConnected(mq));
    teq(events[MQTT_EVENT_CONNECTED], 2);
    teq(fbCount(fb, MQTT_PACKET_CONNECT), 2);
    tcall(mqttPublish(mq, "x", 1, 1, MQTT_WAIT_ACK, "flow/again"), 0);
    closeClient(mq, sock2);
}

/*
    Connecting without waiting for the CONNACK succeeds once the acknowledgement arrives
 */
static void testConnectWaitNone(void)
{
    RSocket *sock;
    Mqtt    *mq;

    memset(events, 0, sizeof(events));
    fbClearPackets(fb);
    sock = fbConnect(fb);
    mq = mqttAlloc("flow-waitnone", eventProc);
    mqttSetKeepAlive(mq, 200);
    tcall(mqttConnect(mq, sock, 0, MQTT_WAIT_NONE), 0);
    WAIT_UNTIL(mqttIsConnected(mq), 2000);
    ttrue(mqttIsConnected(mq));
    WAIT_UNTIL(events[MQTT_EVENT_CONNECTED], 2000);
    teq(events[MQTT_EVENT_CONNECTED], 1);
    //  The keep-alive timer is running
    ttrue(fbWait(fb, MQTT_PACKET_PING, 1, 3000));
    closeClient(mq, sock);

    memset(events, 0, sizeof(events));
    fbClearPackets(fb);
    sock = fbConnect(fb);
    mq = mqttAlloc("flow-waitsent", eventProc);
    tcall(mqttConnect(mq, sock, 0, MQTT_WAIT_SENT), 0);
    WAIT_UNTIL(events[MQTT_EVENT_CONNECTED], 2000);
    teq(events[MQTT_EVENT_CONNECTED], 1);
    tcall(mqttPublish(mq, "x", 1, 1, MQTT_WAIT_ACK, "flow/sent"), 0);
    closeClient(mq, sock);
}


static void onFreeMessage(const MqttRecv *rp)
{
    freed++;
    mqttFree(rp->mq);
}

/*
    Queue two messages in one read so freeing in the first callback leaves receive work pending.
 */
static void testFreeInFastCallback(void)
{
    RSocket *sock;
    Mqtt    *mq;
    uchar   buf[64];
    size_t  len;

    mq = openClient("flow-free-fast", &sock);
    freed = 0;
    tcall(mqttSubscribe(mq, onFreeMessage, 0, MQTT_WAIT_ACK | MQTT_WAIT_FAST, "flow/free"), 0);
    len = fbBuildPublish(buf, "flow/free", "one", 3, 0, 0, 0);
    len += fbBuildPublish(&buf[len], "flow/free", "two", 3, 0, 0, 0);
    fbSend(fb, buf, len);
    WAIT_UNTIL(freed > 0, 2000);
    rSleep(100);
    teq(freed, 1);
    rFreeSocket(sock);
    WAIT_UNTIL(!fb->running, 2000);
}


static void testFreeInFiberCallback(void)
{
    RSocket *sock;
    Mqtt    *mq;

    mq = openClient("flow-free-fiber", &sock);
    freed = 0;
    tcall(mqttSubscribe(mq, onFreeMessage, 0, MQTT_WAIT_ACK, "flow/free"), 0);
    fbPublish(fb, "flow/free", "one", 3, 0, 0, 0);
    WAIT_UNTIL(freed > 0, 2000);
    rSleep(100);
    teq(freed, 1);
    rFreeSocket(sock);
    WAIT_UNTIL(!fb->running, 2000);
}

static void freeOnAttach(Mqtt *mq, int event)
{
    if (event == MQTT_EVENT_ATTACH) {
        freed++;
        mqttFree(mq);
    }
}


static void testFreeOnAttach(void)
{
    Mqtt *mq;

    freed = 0;
    mq = mqttAlloc("flow-free-attach", freeOnAttach);
    ttrue(mqttPublish(mq, "x", 1, 0, MQTT_WAIT_NONE, "flow/free") < 0);
    teq(freed, 1);
}

/*
    Packet identifiers wrap at 65535, skip zero and skip identifiers still in use
 */
static void testMessageIds(void)
{
    RSocket *sock;
    Mqtt    *mq;

    mq = openClient("flow-ids", &sock);
    fb->ackPublish = 0;
    mq->nextId = 0xFFFF;
    tcall(mqttPublish(mq, "a", 1, 1, MQTT_WAIT_SENT, "flow/id"), 0);
    tcall(mqttPublish(mq, "b", 1, 1, MQTT_WAIT_SENT, "flow/id"), 0);
    ttrue(fbWait(fb, MQTT_PACKET_PUBLISH, 2, 2000));
    teq(fbGet(fb, MQTT_PACKET_PUBLISH, 0)->id, 0xFFFF);
    teq(fbGet(fb, MQTT_PACKET_PUBLISH, 1)->id, 1);

    //  65535 and 1 are unacknowledged and still in use
    mq->nextId = 0xFFFF;
    tcall(mqttPublish(mq, "c", 1, 1, MQTT_WAIT_SENT, "flow/id"), 0);
    ttrue(fbWait(fb, MQTT_PACKET_PUBLISH, 3, 2000));
    teq(fbGet(fb, MQTT_PACKET_PUBLISH, 2)->id, 2);
    teq(mqttGetQueueCount(mq), 3);
    fb->ackPublish = 1;
    closeClient(mq, sock);
}

static void fiberMain(void *data)
{
    fb = fbOpen();
    ttrue(fb);

    testQos1Retransmit();
    testQos2PublishWaitAck();
    testQos2Inflight();
    testWaitSent();
    testThrottle();
    testKeepAlivePing();
    testIdleTimeout();
    testOnDemandAttach();
    testNoAttachHandler();
    testFreeWithWaiter();
    testReconnect();
    testConnectWaitNone();
    testMessageIds();
    testFreeInFastCallback();
    testFreeInFiberCallback();
    testFreeOnAttach();

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
