/*
    delivery.tst.c - Message delivery through a real broker: wildcard matching, master
    subscriptions, unsubscribe, fast callbacks and retained messages.

    Topics are prefixed with the process id so concurrent runs never see each other's messages.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "mqtt-test.h"

/*********************************** Locals ***********************************/

static char   prefix[32];
static int    events[8];
static int    countA, countB, countC;
static char   lastTopic[MQTT_MAX_TOPIC_SIZE];
static char   lastData[256];
static size_t lastDataSize;
static int    lastRetain;
static int    fastCount;
static int    fastTerminated;

/************************************ Code ************************************/

static void eventProc(Mqtt *mq, int event)
{
    if (event > 0 && event < 8) {
        events[event]++;
    }
}

static void record(const MqttRecv *rp)
{
    sncopy(lastTopic, sizeof(lastTopic), rp->topic, rp->topicSize);
    lastDataSize = rp->dataSize;
    if (rp->dataSize < sizeof(lastData)) {
        memcpy(lastData, rp->data, rp->dataSize);
        lastData[rp->dataSize] = '\0';
    }
    lastRetain = rp->retain;
}

static void onA(const MqttRecv *rp)
{
    countA++;
    record(rp);
}

static void onB(const MqttRecv *rp)
{
    countB++;
    record(rp);
}

static void onC(const MqttRecv *rp)
{
    countC++;
    record(rp);
}

/*
    Fast callbacks receive the receive buffer directly: the data is not null terminated
 */
static void onFast(const MqttRecv *rp)
{
    fastCount++;
    record(rp);
    fastTerminated = rp->data[rp->dataSize] == '\0';
}

static void reset(void)
{
    memset(events, 0, sizeof(events));
    countA = countB = countC = 0;
    fastCount = 0;
    lastTopic[0] = '\0';
    lastData[0] = '\0';
    lastDataSize = 0;
    lastRetain = 0;
}

static Mqtt *openClient(cchar *id, RSocket **sockp)
{
    Mqtt    *mq;
    RSocket *sock;

    reset();
    sock = connectBroker();
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
}

/*
    Publish a QoS 1 message and wait for the broker's acknowledgement
 */
static void publish(Mqtt *mq, cchar *topic, cchar *data)
{
    tcall(mqttPublish(mq, data, slen(data), 1, MQTT_WAIT_ACK, "%s/%s", prefix, topic), 0);
}

/*
    Local wildcard matching under a master subscription: the broker delivers everything under the
    prefix and the client routes each message to the matching local subscription
 */
static void testWildcardMatching(void)
{
    RSocket *sock;
    Mqtt    *mq;

    mq = openClient("delivery-wild", &sock);
    tcall(mqttSubscribe(mq, onA, 1, MQTT_WAIT_ACK, "%s/+/temp", prefix), 0);
    tcall(mqttSubscribe(mq, onB, 1, MQTT_WAIT_ACK, "%s/x/#", prefix), 0);
    tcall(mqttSubscribe(mq, onC, 1, MQTT_WAIT_ACK, "%s/exact", prefix), 0);
    teq(rGetListLength(mq->topics), 3);

    publish(mq, "a/temp", "one");
    WAIT_UNTIL(countA == 1, 2000);
    teq(countA, 1);
    tmatch(lastData, "one");
    ttrue(sends(lastTopic, "/a/temp"));

    //  Single level wildcard matches exactly one level
    publish(mq, "a/b/temp", "no");
    publish(mq, "temp", "no");
    rSleep(100);
    teq(countA, 1);

    //  Multi-level wildcard matches the parent and every descendant
    publish(mq, "x", "parent");
    WAIT_UNTIL(countB == 1, 2000);
    teq(countB, 1);
    publish(mq, "x/1/2/3", "deep");
    WAIT_UNTIL(countB == 2, 2000);
    teq(countB, 2);
    tmatch(lastData, "deep");

    publish(mq, "exact", "three");
    WAIT_UNTIL(countC == 1, 2000);
    teq(countC, 1);
    publish(mq, "exact/child", "no");
    publish(mq, "nomatch/zzz", "no");
    rSleep(100);
    teq(countA, 1);
    teq(countB, 2);
    teq(countC, 1);

    closeClient(mq, sock);
}

/*
    Unsubscribing one topic leaves the other subscriptions delivering
 */
static void testUnsubscribeKeepsOthers(void)
{
    RSocket *sock;
    Mqtt    *mq;
    char    *topic;

    mq = openClient("delivery-unsub", &sock);
    tcall(mqttSubscribe(mq, onA, 1, MQTT_WAIT_ACK, "%s/keep/a", prefix), 0);
    tcall(mqttSubscribe(mq, onB, 1, MQTT_WAIT_ACK, "%s/keep/b", prefix), 0);
    teq(rGetListLength(mq->topics), 2);

    topic = sfmt("%s/keep/a", prefix);
    tcall(mqttUnsubscribe(mq, topic, MQTT_WAIT_ACK), 0);
    rFree(topic);
    teq(rGetListLength(mq->topics), 1);

    publish(mq, "keep/b", "still");
    WAIT_UNTIL(countB == 1, 2000);
    teq(countB, 1);
    publish(mq, "keep/a", "gone");
    rSleep(100);
    teq(countA, 0);
    closeClient(mq, sock);
}

/*
    Unsubscribing a topic under a master subscription removes the local callback
 */
static void testUnsubscribeUnderMaster(void)
{
    RSocket *sock;
    Mqtt    *mq;
    char    *topic;

    mq = openClient("delivery-master", &sock);
    tcall(mqttSubscribeMaster(mq, 1, MQTT_WAIT_ACK, "%s/m/#", prefix), 0);
    tcall(mqttSubscribe(mq, onA, 1, MQTT_WAIT_ACK, "%s/m/a", prefix), 0);
    tcall(mqttSubscribe(mq, onB, 1, MQTT_WAIT_ACK, "%s/m/b", prefix), 0);
    publish(mq, "m/a", "one");
    WAIT_UNTIL(countA == 1, 2000);
    teq(countA, 1);

    topic = sfmt("%s/m/a", prefix);
    tcall(mqttUnsubscribe(mq, topic, MQTT_WAIT_ACK), 0);
    rFree(topic);
    teq(rGetListLength(mq->topics), 1);
    publish(mq, "m/a", "two");
    publish(mq, "m/b", "three");
    WAIT_UNTIL(countB == 1, 2000);
    teq(countB, 1);
    teq(countA, 1);
    closeClient(mq, sock);
}

/*
    A fast callback sees the message before it is copied: the data is not null terminated
 */
static void testFastCallback(void)
{
    RSocket *sock;
    Mqtt    *mq;

    mq = openClient("delivery-fast", &sock);
    tcall(mqttSubscribe(mq, onFast, 1, MQTT_WAIT_ACK | MQTT_WAIT_FAST, "%s/fast", prefix), 0);
    teq(mqttGetQueueCount(mq), 0);
    publish(mq, "fast", "quick");
    WAIT_UNTIL(fastCount == 1, 2000);
    teq(fastCount, 1);
    teq((int) lastDataSize, 5);
    tmatch(lastData, "quick");
    closeClient(mq, sock);
}

/*
    A retained message is delivered on subscription with the retain flag set
 */
static void testRetained(void)
{
    RSocket *sock;
    Mqtt    *mq;

    mq = openClient("delivery-retain", &sock);
    tcall(mqttPublishRetained(mq, "kept", 4, 1, MQTT_WAIT_ACK, "%s/retained", prefix), 0);
    tcall(mqttSubscribe(mq, onA, 1, MQTT_WAIT_ACK, "%s/retained", prefix), 0);
    WAIT_UNTIL(countA == 1, 2000);
    teq(countA, 1);
    tmatch(lastData, "kept");
    teq(lastRetain, 1);

    //  Clear the retained message so the broker does not keep it
    tcall(mqttPublishRetained(mq, "", 0, 1, MQTT_WAIT_ACK, "%s/retained", prefix), 0);
    closeClient(mq, sock);
}

static void testPingAndDisconnect(void)
{
    RSocket *sock;
    Mqtt    *mq;

    mq = openClient("delivery-ping", &sock);
    tcall(mqttPing(mq), 0);
    WAIT_UNTIL(mqttGetQueueCount(mq) == 0, 2000);
    teq(mqttGetQueueCount(mq), 0);
    ttrue(mqttIsConnected(mq));

    //  The broker closes the connection after a DISCONNECT
    tcall(mqttDisconnect(mq), 0);
    WAIT_UNTIL(events[MQTT_EVENT_DISCONNECT], 2000);
    teq(events[MQTT_EVENT_DISCONNECT], 1);
    tfalse(mqttIsConnected(mq));
    closeClient(mq, sock);
}

static void fiberMain(void *data)
{
    sfmtbuf(prefix, sizeof(prefix), "tm%d", (int) getpid());

    testWildcardMatching();
    testUnsubscribeKeepsOthers();
    testUnsubscribeUnderMaster();
    testFastCallback();
    testRetained();
    testPingAndDisconnect();
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
