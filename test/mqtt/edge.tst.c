/*
    edge.c.tst - MQTT edge cases and error handling tests

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "mqtt.h"

/*********************************** Locals ***********************************/

static void messageCallback(const MqttRecv *resp)
{
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

/************************************ Code ************************************/

static void testMqttThrottle(void)
{
    Mqtt *mq;

    mq = mqttAlloc("test-throttle", NULL);
    ttrue(mq);

    teq(mq->throttle, 0);

    mqttThrottle(mq);
    ttrue(mq->throttle > 0);

    mqttFree(mq);
}

static void testMqttErrorHandling(void)
{
    Mqtt  *mq;
    cchar *error;

    mq = mqttAlloc("test-error", NULL);
    ttrue(mq);

    teq(mq->error, 0);

    error = mqttGetError(mq);
    ttrue(error);

    mqttFree(mq);
}

static void testMqttNullPointers(void)
{
    int   rc;
    cchar *error;
    Ticks activity;

    error = mqttGetError(NULL);
    tfalse(error);

    activity = mqttGetLastActivity(NULL);
    teq(activity, 0);

    rc = mqttMsgsToSend(NULL);
    teq(rc, 0);

    tfalse(mqttIsConnected(NULL));

    mqttSetMessageSize(NULL, 1024);

    mqttSetKeepAlive(NULL, 60 * TPS);

    mqttSetTimeout(NULL, 120 * TPS);

    mqttThrottle(NULL);

    mqttFree(NULL);
}

static void testMqttZeroLengthData(void)
{
    RSocket *sock;
    Mqtt    *mq;
    int     rc;

    sock = createConnectedSocket();
    ttrue(sock);

    mq = mqttAlloc("test-zero", NULL);
    ttrue(mq);

    if (mqttConnect(mq, sock, 0, MQTT_WAIT_ACK) == 0) {
        rc = mqttPublish(mq, "", 0, 0, MQTT_WAIT_NONE, "test/empty");
        teq(rc, 0);

        rc = mqttPublishRetained(mq, "", 0, 0, MQTT_WAIT_NONE, "test/empty/retained");
        teq(rc, 0);
    }

    mqttFree(mq);
    rFreeSocket(sock);
}

static void testMqttEmptyTopic(void)
{
    RSocket *sock;
    Mqtt    *mq;
    char    *msg = "test message";
    int     rc;

    mq = mqttAlloc("test-empty", NULL);
    ttrue(mq);

    sock = createConnectedSocket();
    ttrue(sock);

    rc = mqttConnect(mq, sock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    rc = mqttPublish(mq, msg, slen(msg), 0, MQTT_WAIT_NONE, "");
    teq(rc, R_ERR_BAD_ARGS);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "");
    ttrue(rc < 0);

    mqttFree(mq);
    rFreeSocket(sock);
}

static void testMqttSpecialTopics(void)
{
    RSocket *sock;
    Mqtt    *mq;
    char    *msg = "test message";
    int     rc;

    sock = createConnectedSocket();
    ttrue(sock);

    mq = mqttAlloc("test-special", NULL);
    ttrue(mq);

    rc = mqttConnect(mq, sock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    rc = mqttPublish(mq, msg, slen(msg), 0, MQTT_WAIT_NONE, "\0");
    ttrue(rc < 0);

    rc = mqttPublish(mq, msg, slen(msg), 0, MQTT_WAIT_NONE, " ");
    ttrue(rc < 0);

    rc = mqttPublish(mq, msg, slen(msg), 0, MQTT_WAIT_NONE, "\t");
    ttrue(rc < 0);

    rc = mqttPublish(mq, msg, slen(msg), 0, MQTT_WAIT_NONE, "\n");
    ttrue(rc < 0);

    mqttFree(mq);
    rFreeSocket(sock);
}

static void testMqttWaitFlags(void)
{
    RSocket *sock;
    Mqtt    *mq;
    char    *msg = "test message";
    int     rc;

    sock = createConnectedSocket();
    ttrue(sock);

    mq = mqttAlloc("test-wait", NULL);
    ttrue(mq);

    rc = mqttConnect(mq, sock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    rc = mqttPublish(mq, msg, slen(msg), 0, MQTT_WAIT_NONE, "test/wait/none");
    teq(rc, 0);

    rc = mqttPublish(mq, msg, slen(msg), 1, MQTT_WAIT_SENT, "test/wait/sent1");
    teq(rc, 0);

    rc = mqttPublish(mq, msg, slen(msg), 1, MQTT_WAIT_SENT, "test/wait/sent2");
    teq(rc, 0);

    rc = mqttPublish(mq, msg, slen(msg), 1, MQTT_WAIT_ACK, "test/wait/ack");
    teq(rc, 0);

    // QOS 0 should ignore MQTT_WAIT_ACK
    rc = mqttPublish(mq, msg, slen(msg), 0, MQTT_WAIT_SENT | MQTT_WAIT_ACK, "test/wait/both");
    teq(rc, 0);

    rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_FAST, "test/fast");
    teq(rc, 0);

    mqttFree(mq);
    rFreeSocket(sock);
}

static void testMqttBoundaryValues(void)
{
    RSocket *sock;
    Mqtt    *mq;
    char    *msg;
    int     rc;

    mq = mqttAlloc("test-boundary", NULL);
    ttrue(mq);
    mqttCheckQueue(mq);

    sock = createConnectedSocket();
    ttrue(sock);
    mqttCheckQueue(mq);

    rc = mqttConnect(mq, sock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);

    mqttSetMessageSize(mq, 1);
    msg = "test message";
    rc = mqttPublish(mq, msg, slen(msg), 0, MQTT_WAIT_NONE, "test/small");
    teq(rc, R_ERR_WONT_FIT);

    mqttSetMessageSize(mq, MQTT_MAX_MESSAGE_SIZE);

    mqttSetKeepAlive(mq, 1);
    teq(mq->keepAlive, 1);

    mqttSetKeepAlive(mq, MAXINT64);
    ttrue(mq->keepAlive < MAXINT64);

    mqttSetTimeout(mq, 1);
    teq(mq->timeout, 1);

    mqttSetTimeout(mq, MAXINT64);
    ttrue(mq->timeout < MAXINT64);

    mqttFree(mq);
    rFreeSocket(sock);
}

static void testMqttRepeatedOperations(void)
{
    RSocket *sock;
    Mqtt    *mq;
    char    *msg = "test message";
    int     rc, i;

    sock = createConnectedSocket();
    ttrue(sock);

    mq = mqttAlloc("test-repeated", NULL);
    ttrue(mq);

    rc = mqttConnect(mq, sock, 0, MQTT_WAIT_ACK);
    teq(rc, 0);
    for (i = 0; i < 10; i++) {
        rc = mqttSubscribe(mq, messageCallback, 1, MQTT_WAIT_NONE, "test/repeated/%d", i);
        teq(rc, 0);
    }

    for (i = 0; i < 10; i++) {
        rc = mqttPublish(mq, msg, slen(msg), 0, MQTT_WAIT_NONE, "test/repeated/%d", i);
        teq(rc, 0);
    }

    for (i = 0; i < 10; i++) {
        rc = mqttUnsubscribe(mq, "test/repeated/%d", MQTT_WAIT_NONE);
        teq(rc, 0);
    }
    mqttFree(mq);
    rFreeSocket(sock);
}

static void testMqttLargeClientId(void)
{
    Mqtt *mq;
    char clientId[MQTT_MAX_CLIENT_ID_SIZE];

    memset(clientId, 'C', MQTT_MAX_CLIENT_ID_SIZE - 1);
    clientId[MQTT_MAX_CLIENT_ID_SIZE - 1] = '\0';

    mq = mqttAlloc(clientId, NULL);
    ttrue(mq);
    mqttFree(mq);
}

static void testMqttCredentialsEdgeCases(void)
{
    Mqtt *mq;
    char maxUser[MQTT_MAX_USERNAME_SIZE];
    char maxPass[MQTT_MAX_PASSWORD_SIZE];
    int  rc;

    mq = mqttAlloc("test-creds", NULL);
    ttrue(mq);

    memset(maxUser, 'U', MQTT_MAX_USERNAME_SIZE - 1);
    maxUser[MQTT_MAX_USERNAME_SIZE - 1] = '\0';

    memset(maxPass, 'P', MQTT_MAX_PASSWORD_SIZE - 1);
    maxPass[MQTT_MAX_PASSWORD_SIZE - 1] = '\0';

    rc = mqttSetCredentials(mq, maxUser, maxPass);
    teq(rc, 0);

    rc = mqttSetCredentials(mq, "", "");
    teq(rc, 0);

    mqttFree(mq);
}

static void testMqttWillEdgeCases(void)
{
    Mqtt *mq;
    char maxTopic[MQTT_MAX_TOPIC_SIZE];
    char largeMsg[1024];
    int  rc;

    mq = mqttAlloc("test-will", NULL);
    ttrue(mq);

    memset(maxTopic, 'T', MQTT_MAX_TOPIC_SIZE - 1);
    maxTopic[MQTT_MAX_TOPIC_SIZE - 1] = '\0';

    memset(largeMsg, 'M', sizeof(largeMsg));

    rc = mqttSetWill(mq, maxTopic, largeMsg, sizeof(largeMsg));
    teq(rc, 0);

    rc = mqttSetWill(mq, "topic", "", 0);
    teq(rc, R_ERR_BAD_ARGS);

    rc = mqttSetWill(mq, "topic", "msg", 1);
    teq(rc, 0);

    mqttFree(mq);
}

static void fiberMain(void *data)
{
    testMqttThrottle();
    testMqttErrorHandling();
    testMqttNullPointers();
    testMqttZeroLengthData();
    testMqttEmptyTopic();
    testMqttSpecialTopics();
    testMqttWaitFlags();
    testMqttBoundaryValues();
    testMqttRepeatedOperations();
    testMqttLargeClientId();
    testMqttCredentialsEdgeCases();
    testMqttWillEdgeCases();
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