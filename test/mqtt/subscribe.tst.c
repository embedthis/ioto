/*
    subscribe.c.tst - MQTT subscription tests

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "mqtt.h"
#include    <stdbool.h>

/*********************************** Locals ***********************************/

static int     callbackCount = 0;
static bool    testComplete = false;
static RSocket *testSock = NULL;

static void messageCallback(const MqttRecv *resp)
{
    callbackCount++;
    testComplete = true;
}

static RSocket *createConnectedSocket(void)
{
    RSocket *sock = rAllocSocket();

    if (rConnectSocket(sock, "localhost", 1883, 0) < 0) {
        rFreeSocket(sock);
        return NULL;
    }
    return sock;
}

static void closeTestSocket(void)
{
    if (testSock) {
        rFreeSocket(testSock);
        testSock = NULL;
    }
}

/*
    Publisher fiber to publish a message to the test/subscribe topic and then exit
 */
static void publisherFiber(void)
{
    RSocket *sock;
    Mqtt    *mq;
    char    *testMsg;
    int     rc;

    sock = createConnectedSocket();
    ttrue(sock);

    mq = mqttAlloc("test-publisher", NULL);
    ttrue(mq);

    rc = mqttConnect(mq, sock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    testMsg = "test subscribe message";
    rc = mqttPublish(mq, testMsg, slen(testMsg), 1, MQTT_WAIT_ACK, "test/subscribe");
    teq(rc, 0);

    mqttFree(mq);
    rFreeSocket(sock);
}

/************************************ Code ************************************/

static void testMqttSubscribeNullMq(void)
{
    int rc;

    rc = mqttSubscribe(NULL, messageCallback, 1, MQTT_WAIT_NONE, "test/topic");
    teq(rc, R_ERR_BAD_ARGS);
}

static void testMqttSubscribeValidTopic(void)
{
    Mqtt *mq;
    int  rc;
    int  previousCount = callbackCount;

    testSock = createConnectedSocket();
    ttrue(testSock);

    mq = mqttAlloc("test-sub-client", NULL);
    ttrue(mq);

    rc = mqttConnect(mq, testSock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "test/subscribe");
    teq(rc, 0);

    testComplete = false;
    rStartEvent((REventProc) publisherFiber, 0, 0);

    int timeout = 5 * TPS;
    while (!testComplete && timeout-- > 0) {
        rSleep(10);
    }
    ttrue(callbackCount > previousCount);

    mqttFree(mq);
    closeTestSocket();
}

static void testMqttSubscribeLongTopic(void)
{
    Mqtt *mq;
    char longTopic[MQTT_MAX_TOPIC_SIZE + 10];
    int  rc;

    testSock = createConnectedSocket();
    ttrue(testSock);

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    rc = mqttConnect(mq, testSock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    memset(longTopic, 'T', sizeof(longTopic));
    longTopic[sizeof(longTopic) - 1] = '\0';

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, longTopic);
    teq(rc, R_ERR_BAD_ARGS);

    mqttFree(mq);
    closeTestSocket();
}

static void testMqttSubscribeWildcards(void)
{
    Mqtt *mq;
    int  rc;

    testSock = createConnectedSocket();
    ttrue(testSock);

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    rc = mqttConnect(mq, testSock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "test/+");
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "test/#");
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "+/status");
    teq(rc, 0);

    mqttFree(mq);
    closeTestSocket();
}

static void testMqttSubscribeFormattedTopic(void)
{
    Mqtt *mq;
    int  rc;

    testSock = createConnectedSocket();
    ttrue(testSock);

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    rc = mqttConnect(mq, testSock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "device/%s/status", "123");
    teq(rc, 0);

    mqttFree(mq);
    closeTestSocket();
}

static void testMqttSubscribeQos0(void)
{
    Mqtt *mq;
    int  rc;

    testSock = createConnectedSocket();
    ttrue(testSock);

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    rc = mqttConnect(mq, testSock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 0, MQTT_WAIT_NONE, "test/qos0");
    teq(rc, 0);

    mqttFree(mq);
    closeTestSocket();
}

static void testMqttSubscribeQos1(void)
{
    Mqtt *mq;
    int  rc;

    testSock = createConnectedSocket();
    ttrue(testSock);

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    rc = mqttConnect(mq, testSock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "test/qos1");
    teq(rc, 0);

    mqttFree(mq);
    closeTestSocket();
}

static void testMqttSubscribeQos2(void)
{
    Mqtt *mq;
    int  rc;

    testSock = createConnectedSocket();
    ttrue(testSock);

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    rc = mqttConnect(mq, testSock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 2, MQTT_WAIT_NONE, "test/qos2");
    teq(rc, 0);

    mqttFree(mq);
    closeTestSocket();
}

static void testMqttSubscribeMaster(void)
{
    Mqtt *mq;
    int  rc;

    testSock = createConnectedSocket();
    ttrue(testSock);

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    rc = mqttConnect(mq, testSock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    rc = mqttSubscribeMaster(mq, 1, MQTT_WAIT_NONE, "device/+");
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "device/123/status");
    teq(rc, 0);

    mqttFree(mq);
    closeTestSocket();
}

static void testMqttSubscribeMasterLongTopic(void)
{
    Mqtt *mq;
    char longTopic[MQTT_MAX_TOPIC_SIZE + 10];
    int  rc;

    testSock = createConnectedSocket();
    ttrue(testSock);

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    rc = mqttConnect(mq, testSock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    memset(longTopic, 'T', sizeof(longTopic));
    longTopic[sizeof(longTopic) - 1] = '\0';

    rc = mqttSubscribeMaster(mq, 1, MQTT_WAIT_NONE, longTopic);
    teq(rc, R_ERR_BAD_ARGS);

    mqttFree(mq);
    closeTestSocket();
}

static void testMqttUnsubscribe(void)
{
    Mqtt *mq;
    int  rc;

    testSock = createConnectedSocket();
    ttrue(testSock);

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    rc = mqttConnect(mq, testSock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "test/topic");
    teq(rc, 0);

    rc = mqttUnsubscribe(mq, "test/topic", MQTT_WAIT_NONE);
    teq(rc, 0);

    mqttFree(mq);
    closeTestSocket();
}

static void testMqttUnsubscribeMaster(void)
{
    Mqtt *mq;
    int  rc;

    testSock = createConnectedSocket();
    ttrue(testSock);

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    rc = mqttConnect(mq, testSock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    rc = mqttSubscribeMaster(mq, 1, MQTT_WAIT_NONE, "device/+");
    teq(rc, 0);

    rc = mqttUnsubscribeMaster(mq, "device/+", MQTT_WAIT_NONE);
    teq(rc, 0);

    mqttFree(mq);
    closeTestSocket();
}

static void testMqttSubscribeNullCallback(void)
{
    Mqtt *mq;
    int  rc;

    testSock = createConnectedSocket();
    ttrue(testSock);

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    rc = mqttConnect(mq, testSock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    rc = mqttSubscribe(mq, NULL, 1, MQTT_WAIT_NONE, "test/topic");
    teq(rc, 0);

    mqttFree(mq);
    closeTestSocket();
}

static void testMqttSubscribeMultipleTopics(void)
{
    Mqtt *mq;
    int  rc;

    testSock = createConnectedSocket();
    ttrue(testSock);

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    rc = mqttConnect(mq, testSock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "topic1");
    teq(rc, 0);
    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "topic2");
    teq(rc, 0);
    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "topic3");
    teq(rc, 0);

    mqttFree(mq);
    closeTestSocket();
}

static void fiberMain(void *data)
{
    testMqttSubscribeNullMq();
    testMqttSubscribeValidTopic();
    testMqttSubscribeLongTopic();
    testMqttSubscribeWildcards();
    testMqttSubscribeFormattedTopic();
    testMqttSubscribeQos0();
    testMqttSubscribeQos1();
    testMqttSubscribeQos2();
    testMqttSubscribeMaster();
    testMqttSubscribeMasterLongTopic();
    testMqttUnsubscribe();
    testMqttUnsubscribeMaster();
    testMqttSubscribeNullCallback();
    testMqttSubscribeMultipleTopics();
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
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */