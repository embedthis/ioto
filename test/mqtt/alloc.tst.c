/*
    alloc.c.tst - MQTT allocation and lifecycle tests

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "testme.h"
#include    "mqtt.h"

/************************************ Code ************************************/

static void eventProc(Mqtt *mq, int event)
{
}

static void testMqttAlloc(void)
{
    Mqtt *mq;

    mq = mqttAlloc("test-client", eventProc);
    ttrue(mq);
    ttrue(mq->id);
    tmatch(mq->id, "test-client");
    ttrue(mq->proc == eventProc);
    teq(mq->connected, 0);
    teq(mq->error, 0);
    ttrue(mq->buf);
    ttrue(mq->topics);
    teq(mq->keepAlive, MQTT_KEEP_ALIVE);
    teq(mq->timeout, MQTT_TIMEOUT);
    teq(mq->maxMessage, MQTT_MAX_MESSAGE_SIZE);
    mqttFree(mq);
}

static void testMqttAllocNullClient(void)
{
    Mqtt *mq;

    mq = mqttAlloc(NULL, eventProc);
    tfalse(mq);

    mq = mqttAlloc("", eventProc);
    tfalse(mq);
}

static void testMqttAllocNullProc(void)
{
    Mqtt *mq;

    mq = mqttAlloc("test-client", NULL);
    ttrue(mq);
}

static void testMqttAllocLongClientId(void)
{
    Mqtt *mq;
    char longId[MQTT_MAX_CLIENT_ID_SIZE + 10];

    memset(longId, 'A', sizeof(longId));
    longId[sizeof(longId) - 1] = '\0';

    mq = mqttAlloc(longId, eventProc);
    tfalse(mq);
}

static void testMqttFree(void)
{
    Mqtt *mq;

    mq = mqttAlloc("test-client", eventProc);
    ttrue(mq);

    mqttFree(mq);
    mqttFree(NULL);
}

static void testMqttSetCredentials(void)
{
    Mqtt *mq;
    int  rc;

    mq = mqttAlloc("test-client", eventProc);
    ttrue(mq);

    rc = mqttSetCredentials(mq, "user", "pass");
    teq(rc, 0);
    tmatch(mq->username, "user");
    tmatch(mq->password, "pass");

    rc = mqttSetCredentials(mq, NULL, NULL);
    teq(rc, 0);

    mqttFree(mq);
}

static void testMqttSetCredentialsTooLong(void)
{
    Mqtt *mq;
    char longUser[MQTT_MAX_USERNAME_SIZE + 10];
    char longPass[MQTT_MAX_PASSWORD_SIZE + 10];
    int  rc;

    mq = mqttAlloc("test-client", eventProc);
    ttrue(mq);

    memset(longUser, 'U', sizeof(longUser));
    longUser[sizeof(longUser) - 1] = '\0';

    rc = mqttSetCredentials(mq, longUser, "pass");
    teq(rc, R_ERR_BAD_ARGS);

    memset(longPass, 'P', sizeof(longPass));
    longPass[sizeof(longPass) - 1] = '\0';

    rc = mqttSetCredentials(mq, "user", longPass);
    teq(rc, R_ERR_BAD_ARGS);

    mqttFree(mq);
}

static void testMqttSetWill(void)
{
    Mqtt *mq;
    char *willMsg = "device offline";
    int  rc;

    mq = mqttAlloc("test-client", eventProc);
    ttrue(mq);

    rc = mqttSetWill(mq, "device/status", willMsg, slen(willMsg));
    teq(rc, 0);
    tmatch(mq->willTopic, "device/status");
    teq(mq->willMsgSize, slen(willMsg));

    mqttFree(mq);
}

static void testMqttSetWillLongTopic(void)
{
    Mqtt *mq;
    char longTopic[MQTT_MAX_TOPIC_SIZE + 10];
    char *willMsg = "offline";
    int  rc;

    mq = mqttAlloc("test-client", eventProc);
    ttrue(mq);

    memset(longTopic, 'T', sizeof(longTopic));
    longTopic[sizeof(longTopic) - 1] = '\0';

    rc = mqttSetWill(mq, longTopic, willMsg, slen(willMsg));
    teq(rc, R_ERR_BAD_ARGS);

    mqttFree(mq);
}

static void testMqttSetKeepAlive(void)
{
    Mqtt *mq;

    mq = mqttAlloc("test-client", eventProc);
    ttrue(mq);

    mqttSetKeepAlive(mq, 60 * TPS);
    teq(mq->keepAlive, 60 * TPS);

    mqttSetKeepAlive(mq, 0);
    teq(mq->keepAlive, MQTT_KEEP_ALIVE);

    mqttSetKeepAlive(mq, -1);
    teq(mq->keepAlive, MQTT_KEEP_ALIVE);

    mqttFree(mq);
}

static void testMqttSetTimeout(void)
{
    Mqtt *mq;

    mq = mqttAlloc("test-client", eventProc);
    ttrue(mq);

    mqttSetTimeout(mq, 120 * TPS);
    teq(mq->timeout, 120 * TPS);

    mqttSetTimeout(mq, 0);
    ttrue(mq->timeout >= MAXINT64 / 10);

    mqttSetTimeout(mq, -1);
    teq(mq->timeout, MQTT_TIMEOUT);

    mqttFree(mq);
}

static void testMqttSetMessageSize(void)
{
    Mqtt *mq;

    mq = mqttAlloc("test-client", eventProc);
    ttrue(mq);

    mqttSetMessageSize(mq, 128 * 1024);
    teq(mq->maxMessage, 128 * 1024);

    mqttFree(mq);
}

static void testMqttIsConnected(void)
{
    Mqtt *mq;

    mq = mqttAlloc("test-client", eventProc);
    ttrue(mq);

    tfalse(mqttIsConnected(mq));

    tfalse(mqttIsConnected(NULL));

    mqttFree(mq);
}

static void fiberMain(void *data)
{
    testMqttAlloc();
    testMqttAllocNullClient();
    testMqttAllocNullProc();
    testMqttAllocLongClientId();
    testMqttFree();
    testMqttSetCredentials();
    testMqttSetCredentialsTooLong();
    testMqttSetWill();
    testMqttSetWillLongTopic();
    testMqttSetKeepAlive();
    testMqttSetTimeout();
    testMqttSetMessageSize();
    testMqttIsConnected();
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