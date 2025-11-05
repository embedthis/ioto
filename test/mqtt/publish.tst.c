/*
    publish.c.tst - MQTT publish tests

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "mqtt.h"

/*********************************** Code *************************************/

static RSocket *testSock = NULL;

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

/************************************ Code ************************************/

static void testMqttPublishNullMq(void)
{
    char *msg = "test message";
    int  rc;

    rc = mqttPublish(NULL, msg, slen(msg), 0, MQTT_WAIT_NONE, "test/topic");
    teq(rc, R_ERR_BAD_ARGS);
}

static void testMqttPublishNullTopic(void)
{
    Mqtt *mq;
    char *msg = "test message";
    int  rc;

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    testSock = createConnectedSocket();
    ttrue(testSock);

    rc = mqttConnect(mq, testSock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    rc = mqttPublish(mq, msg, slen(msg), 0, MQTT_WAIT_NONE, NULL);
    teq(rc, R_ERR_BAD_ARGS);

    mqttFree(mq);
    closeTestSocket();
}

static void testMqttPublishLongTopic(void)
{
    Mqtt *mq;
    char *msg = "test message";
    char longTopic[MQTT_MAX_TOPIC_SIZE + 10];
    int  rc;

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    testSock = createConnectedSocket();
    ttrue(testSock);

    rc = mqttConnect(mq, testSock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    memset(longTopic, 'T', sizeof(longTopic));
    longTopic[sizeof(longTopic) - 1] = '\0';

    rc = mqttPublish(mq, msg, slen(msg), 0, MQTT_WAIT_NONE, longTopic);
    teq(rc, R_ERR_BAD_ARGS);

    mqttFree(mq);
    closeTestSocket();
}

static void testMqttPublishBadQos(void)
{
    Mqtt *mq;
    char *msg = "test message";
    int  rc;

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    testSock = createConnectedSocket();
    ttrue(testSock);

    rc = mqttConnect(mq, testSock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    rc = mqttPublish(mq, msg, slen(msg), 3, MQTT_WAIT_NONE, "test/topic");
    teq(rc, R_ERR_BAD_ARGS);

    rc = mqttPublish(mq, msg, slen(msg), -1, MQTT_WAIT_NONE, "test/topic");
    teq(rc, R_ERR_BAD_ARGS);

    mqttFree(mq);
    closeTestSocket();
}

static void testMqttPublishLargeMessage(void)
{
    Mqtt *mq;
    char *largeMsg;
    int  rc;

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    testSock = createConnectedSocket();
    ttrue(testSock);

    mqttSetMessageSize(mq, 1024);

    largeMsg = rAlloc(2048);
    ttrue(largeMsg);

    rc = mqttConnect(mq, testSock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    memset(largeMsg, 'M', 2048);

    rc = mqttPublish(mq, largeMsg, 2048, 0, MQTT_WAIT_NONE, "test/topic");
    teq(rc, R_ERR_WONT_FIT);

    rFree(largeMsg);
    mqttFree(mq);
    closeTestSocket();
}

static void testMqttPublishNegativeSize(void)
{
    Mqtt *mq;
    char *msg = "test message";
    int  rc;

    testSock = createConnectedSocket();
    ttrue(testSock);

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    rc = mqttConnect(mq, testSock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    rc = mqttPublish(mq, msg, 0, 0, MQTT_WAIT_NONE, "test/topic");
    teq(rc, 0);

    mqttFree(mq);
    closeTestSocket();
}

static void testMqttPublishRetained(void)
{
    Mqtt *mq;
    char *msg = "retained message";
    int  rc;

    testSock = createConnectedSocket();
    ttrue(testSock);

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    rc = mqttConnect(mq, testSock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    rc = mqttPublishRetained(mq, msg, slen(msg), 1, MQTT_WAIT_NONE, "test/retained");
    teq(rc, 0);

    mqttFree(mq);
    closeTestSocket();
}

static void testMqttPublishRetainedNullMq(void)
{
    char *msg = "test message";
    int  rc;

    rc = mqttPublishRetained(NULL, msg, slen(msg), 0, MQTT_WAIT_NONE, "test/topic");
    teq(rc, R_ERR_BAD_ARGS);
}

static void testMqttPublishRetainedNullTopic(void)
{
    Mqtt *mq;
    char *msg = "test message";
    int  rc;

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    rc = mqttPublishRetained(mq, msg, slen(msg), 0, MQTT_WAIT_NONE, NULL);
    teq(rc, R_ERR_BAD_ARGS);

    mqttFree(mq);
}

static void testMqttPublishFormattedTopic(void)
{
    Mqtt *mq;
    char *msg = "test message";
    int  rc;

    testSock = createConnectedSocket();
    if (!testSock) {
        return;
    }

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    if (mqttConnect(mq, testSock, 0, MQTT_WAIT_ACK) == 0) {
        rc = mqttPublish(mq, msg, slen(msg), 0, MQTT_WAIT_NONE, "device/%s/status", "123");
        teq(rc, 0);
    }

    mqttFree(mq);
    closeTestSocket();
}

static void testMqttPublishQos0(void)
{
    Mqtt *mq;
    char *msg = "qos0 message";
    int  rc;

    testSock = createConnectedSocket();
    if (!testSock) {
        return;
    }

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    if (mqttConnect(mq, testSock, 0, MQTT_WAIT_ACK) == 0) {
        rc = mqttPublish(mq, msg, slen(msg), 0, MQTT_WAIT_NONE, "test/qos0");
        teq(rc, 0);
    }

    mqttFree(mq);
    closeTestSocket();
}

static void testMqttPublishQos1(void)
{
    Mqtt *mq;
    char *msg = "qos1 message";
    int  rc;

    testSock = createConnectedSocket();
    if (!testSock) {
        return;
    }

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    if (mqttConnect(mq, testSock, 0, MQTT_WAIT_ACK) == 0) {
        rc = mqttPublish(mq, msg, slen(msg), 1, MQTT_WAIT_NONE, "test/qos1");
        teq(rc, 0);
    }

    mqttFree(mq);
    closeTestSocket();
}

static void testMqttPublishQos2(void)
{
    Mqtt *mq;
    char *msg = "qos2 message";
    int  rc;

    testSock = createConnectedSocket();
    if (!testSock) {
        return;
    }

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);

    if (mqttConnect(mq, testSock, 0, MQTT_WAIT_ACK) == 0) {
        rc = mqttPublish(mq, msg, slen(msg), 2, MQTT_WAIT_NONE, "test/qos2");
        teq(rc, 0);
    }

    mqttFree(mq);
    closeTestSocket();
}

static void fiberMain(void *data)
{
    testMqttPublishNullMq();
    testMqttPublishNullTopic();
    testMqttPublishLongTopic();
    testMqttPublishBadQos();
    testMqttPublishLargeMessage();
    testMqttPublishNegativeSize();
    testMqttPublishRetained();
    testMqttPublishRetainedNullMq();
    testMqttPublishRetainedNullTopic();
    testMqttPublishFormattedTopic();
    testMqttPublishQos0();
    testMqttPublishQos1();
    testMqttPublishQos2();
    rStop();
}

int main(void)
{
    rInit((RFiberProc) fiberMain, 0);
    rServiceEvents();
    rTerm();
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This is proprietary software and requires a commercial license from the author.
 */
